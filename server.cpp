// server.cpp

#include <algorithm>
#include <arpa/inet.h>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <sstream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#define PORT "1337"

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

void handle_client_data(int listener, epoll_event& event, int& epollfd,
    std::vector<int>& connections)
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
        // TODO: optimise this O(n) into O(1)
        connections.erase(
            std::find(connections.begin(), connections.end(), sender_fd));

        epoll_ctl(epollfd, EPOLL_CTL_DEL, sender_fd, nullptr);
        close(sender_fd);
    } else {
        std::cout << "pollserver: socket received " << bytes_rec
                  << " bytes from fd " << sender_fd << " :" << buf << "\n";

        std::stringstream ss;
        ss << sender_fd << ": " << buf;
        std::string msg = ss.str();

        for (const auto& dest_fd : connections) {
            if (dest_fd != listener && dest_fd != sender_fd) {
                if (send(dest_fd, msg.c_str(), msg.length(), 0) == -1) {
                    std::cerr << "send error\n";
                }
            }
        }
    }
}

void process_connections(int listener, std::vector<struct epoll_event>& events,
    int& n, int& epollfd, std::vector<int>& connections)
{

    for (int i {}; i < n; i++) {
        if (events[i].events & (EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
            if (events[i].data.fd == listener) {
                handle_new_connection(listener, epollfd, connections);
            } else {
                handle_client_data(listener, events[i], epollfd, connections);
            }
        }
    }
}

int main(void)
{
    int listener {};

    show_hostname();
    listener = get_listener_socket();
    setnonblocking(listener);

    int epollfd = epoll_create1(0);
    if (epollfd == -1) {
        std::cerr << "epoll_create1\n";
        std::exit(1);
    }

    std::vector<int> connections { listener };
    std::vector<struct epoll_event> events(64);

    {
        struct epoll_event ev
            = { .events = EPOLLIN, .data = { .fd = listener } };
        epoll_ctl(epollfd, EPOLL_CTL_ADD, listener, &ev);
    }

    std::cout << "pollserver is waiting for connections\n";
    for (;;) {
        int n {};
        if ((n = epoll_wait(epollfd, events.data(), events.size(), -1)) == -1) {
            std::cerr << "poll error\n";
            exit(1);
        }
        process_connections(listener, events, n, epollfd, connections);
    }
    close(listener);
    return 0;
}
