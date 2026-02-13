// server.cpp

#include "server.hpp"
#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <iostream>
#include <memory>

#define PORT "1337"

void run_server(int epollfd, std::vector<struct epoll_event>& events,
    int& listener, std::vector<int>& connections,
    std::unique_ptr<Ringbuffer<q_order, CAP>>& ring_buffer)
{
    int n {};
    if ((n = epoll_wait(epollfd, events.data(), events.size(), -1)) == -1) {
        std::cerr << "poll error\n";
        exit(1);
    }
    process_connections(listener, events, n, epollfd, connections, ring_buffer);
}

int setnonblocking(int sockfd)
{
    if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK) == -1) {
        std::cerr << "[CRITICAL] failed to set non blocking\n";
        return -1;
    }
    return 0;
}

void show_hostname()
{
    char hostname[1024];
    if (gethostname(hostname, sizeof hostname) == -1) {
        std::cerr << "hostname error\n";
        std::exit(1);
    }
    std::cout << "server.cpp running on " << hostname << "\n";
    return;
}

int get_listener_socket()
{
    struct addrinfo hints {}, *res;
    int sockfd {};

    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE;

    int status {};
    if ((status = getaddrinfo(NULL, PORT, &hints, &res)) != 0) {
        std::cerr << "gai error: " << gai_strerror(status) << "\n";
        std::exit(1);
    }
    struct addrinfo* p;
    for (p = res; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            std::cerr << "socket error\n";
            continue;
        }

        int yes { 1 };
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes)
            == -1) {
            std::cerr << "sso error\n";
            std::exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            std::cerr << "bind error\n";
            close(sockfd);
            continue;
        };

        break;
    }

    if (!p) {
        std::cerr << "no good res found, exiting\n";
        std::exit(1);
    }

    freeaddrinfo(res);

    std::cout << "Listening for connections...\n";
    if (listen(sockfd, 5) == -1) {
        std::cerr << "failed to listen, exiting\n";
        std::exit(1);
    }
    return sockfd;
}

std::string inet_ntop2(void* addr)
{
    struct sockaddr_storage* sas = (struct sockaddr_storage*)addr;
    char buf[INET6_ADDRSTRLEN];
    void* src;

    switch (sas->ss_family) {
    case AF_INET:
        src = &(((struct sockaddr_in*)addr)->sin_addr);
        break;
    case AF_INET6:
        src = &(((struct sockaddr_in6*)addr)->sin6_addr);
        break;
    default:
        return "";
    }
    if (inet_ntop(sas->ss_family, src, buf, sizeof buf) != nullptr) {
        return std::string(buf);
    } else {
        return "";
    }
}

void handle_new_connection(
    int listener, int& epollfd, std::vector<int>& connections)
{
    struct sockaddr_storage their_addr;
    socklen_t addr_size = sizeof their_addr;

    int new_fd = accept(listener, (struct sockaddr*)&their_addr, &addr_size);
    std::cout << "Connection accepted\n";
    if (new_fd == -1) {
        std::cerr << "failed to accept\n";
    } else {
        setnonblocking(new_fd);
        connections.push_back(new_fd);
        struct epoll_event ev = { .events = EPOLLIN, .data = { .fd = new_fd } };
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, new_fd, &ev) == -1) {
            std::cerr << "epoll_ctl failed\n";
            std::exit(1);
        }
        std::cout << "[SERVER] new connection from " << inet_ntop2(&their_addr)
                  << " on socket " << new_fd << "\n";
    }
}

void handle_client_data(epoll_event& event, int& epollfd,
    std::vector<int>& connections,
    std::unique_ptr<Ringbuffer<q_order, CAP>>& ring_buffer)
{
    char buf[1024];
    int sender_fd = event.data.fd;
    int bytes_rec = recv(sender_fd, buf, sizeof buf - 1, 0);
    if (bytes_rec <= 0) {
        if (bytes_rec == 0) {
            std::cout << "pollserver: socket " << event.data.fd
                      << " hung up.\n";
        } else {
            std::cerr << "recv error\n";
        }
        // NOTE: std::vector for small connections
        connections.erase(
            std::find(connections.begin(), connections.end(), sender_fd));

        epoll_ctl(epollfd, EPOLL_CTL_DEL, sender_fd, nullptr);
        close(sender_fd);
    } else {
        std::string str(buf, bytes_rec);

        std::cout << "pollserver: socket received " << bytes_rec
                  << " bytes from fd " << sender_fd << " : " << str << "\n";

        q_order order {};
        if (parse_order(str, order)) {
            ring_buffer->push(order);
        } else {
            std::cerr << "failed to parse order";
        }
    }
}

void process_connections(int listener, std::vector<struct epoll_event>& events,
    int& n, int& epollfd, std::vector<int>& connections,
    std::unique_ptr<Ringbuffer<q_order, CAP>>& ring_buffer)
{
    for (int i {}; i < n; i++) {
        if (events[i].events & (EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
            if (events[i].data.fd == listener) {
                handle_new_connection(listener, epollfd, connections);
            } else {
                handle_client_data(
                    events[i], epollfd, connections, ring_buffer);
            }
        }
    }
}

const char* skip_ws(const char* p, const char* end)
{
    while (p < end && std::isspace((unsigned char)*p))
        ++p;
    return p;
}

bool parse_order(std::string_view sv, q_order& order)
{
    const char* p = sv.data();
    const char* end = sv.data() + sv.size();

    p = skip_ws(p, end);

    bool b {};
    {
        int temp;
        auto [ptr, ec] = std::from_chars(p, end, temp);
        if (ec != std::errc() || !(temp == 1 || temp == 0))
            return false;
        b = (temp == 1);
        p = ptr;
    }

    p = skip_ws(p, end);

    double price {};
    {
        double temp;
        auto [ptr, ec]
            = std::from_chars(p, end, temp, std::chars_format::general);
        if (ec != std::errc())
            return false;
        price = temp;
        p = ptr;
    }

    p = skip_ws(p, end);

    uint32_t qty {};
    {
        uint32_t temp;
        auto [ptr, ec] = std::from_chars(p, end, temp);
        if (ec != std::errc())
            return false;
        qty = temp;
        p = ptr;
    }

    p = skip_ws(p, end);

    if (p != end)
        return false;

    order.is_buy = b;
    order.price = price;
    order.quantity = qty;

    return true;
}
