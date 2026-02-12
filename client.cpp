// client.cpp

#include "client.hpp"
#include <iostream>
#include <string.h>
#include <string>
#include <string_view>
#include <thread>

#define PORT "1337"

int main(void)
{
    show_hostname();

    struct addrinfo hints, *res;
    int sockfd;

    memset(&hints, 0, sizeof hints);
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, PORT, &hints, &res) == -1) {
        fprintf(stderr, "gai error\n");
        exit(1);
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    if (sockfd == -1) {
        fprintf(stderr, "socket error\n");
        exit(1);
    }

    printf("Connecting to server...\n");
    if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
        fprintf(stderr, "socket error\n");
        exit(1);
    }
    printf("Connected to server.\n");
    freeaddrinfo(res);

    std::stop_source ss;
    std::jthread t_send(
        [&](std::stop_token st) { thread_send(st, ss, sockfd); });
    std::jthread t_recv(
        [&](std::stop_token st) { thread_recv(st, ss, sockfd); });

    while (!ss.stop_requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    close(sockfd);
    return 0;
}

void show_hostname()
{
    char hostname[1024];
    if (gethostname(hostname, sizeof hostname) == -1) {
        std::cerr << "hostname error\n";
        std::exit(1);
    }
    std::cout << "client.cpp running on " << hostname << "\n";
    return;
}

void request_stop_and_close(std::stop_source& ss, const int& sockfd)
{
    ss.request_stop();
    ::shutdown(sockfd, SHUT_RDWR);
    // close(sockfd);
    // shutdown is superior as it avoids ^D bug
}

void thread_send(std::stop_token st, std::stop_source& ss, const int& sockfd)
{
    while (!st.stop_requested()) {
        std::cout << "Me: " << std::flush;
        std::string s;
        if (!std::getline(std::cin, s)) {
            request_stop_and_close(ss, sockfd);
            return;
        }
        if (send(sockfd, s.c_str(), s.length(), 0) == -1) {
            request_stop_and_close(ss, sockfd);
            return;
        }
    }
}

void thread_recv(std::stop_token st, std::stop_source& ss, const int& sockfd)
{
    while (!st.stop_requested()) {
        char buf[1024];
        int bytes_recv = recv(sockfd, buf, sizeof(buf) - 1, 0);
        if (bytes_recv <= 0) {
            if (bytes_recv == 0) {
            } else {
                std::cerr << "recv error\n";
            }
            request_stop_and_close(ss, sockfd);
            return;
        }
        std::cout << std::string_view(buf) << std::endl;
    }
}
