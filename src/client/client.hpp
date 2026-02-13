// client.hpp
#pragma once

#include <netdb.h>
#include <stdlib.h>
#include <stop_token>
#include <sys/socket.h>
#include <unistd.h>

void show_hostname();
void request_stop_and_close(std::stop_source& ss, const int& sockfd);
void thread_send(std::stop_token st, std::stop_source& ss, const int& sockfd);
void thread_recv(std::stop_token st, std::stop_source& ss, const int& sockfd);
