// server.hpp
#pragma once

#include "ringbuffer.hpp"
#include <arpa/inet.h>
#include <fcntl.h>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

struct q_order {
        bool is_buy;
        double price;
        uint32_t quantity;
};

void run_server(int epollfd, std::vector<struct epoll_event>& events, int& listener,
    std::vector<int>& connections, std::unique_ptr<Ringbuffer<q_order, CAP>>& ring_buffer);
int setnonblocking(int sockfd);
void show_hostname();
int get_listener_socket();
std::string inet_ntop2(void* addr);
void handle_new_connection(int listener, int& epollfd, std::vector<int>& connections);
void handle_client_data(epoll_event& event, int& epollfd, std::vector<int>& connections,
    std::unique_ptr<Ringbuffer<q_order, CAP>>& ring_buffer);
void process_connections(int listener, std::vector<struct epoll_event>& events, int& n,
    int& epollfd, std::vector<int>& connections,
    std::unique_ptr<Ringbuffer<q_order, CAP>>& ring_buffer);
const char* skip_ws(const char* p, const char* end);
bool parse_order(std::string_view sv, q_order& order);
