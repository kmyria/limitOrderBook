// gateway.cpp

#include "orderbook.hpp"
#include "fastqueue.hpp"
#include "server.hpp"
#include <chrono>
#include <csignal>
#include <cstddef>
#include <iostream>
#include <memory>
#include <stop_token>
#include <sys/epoll.h>
#include <thread>
#include <vector>

namespace {
volatile std::sig_atomic_t g_stop_requested = 0;
}

void network_io_worker(std::stop_token st, struct state& gate, std::vector<int>& connections);
void matching_engine_worker(std::stop_token st, struct state& gate);

void signal_handler(int)
{
        g_stop_requested = 1;
}

struct state {
        int listener { };
        int epollfd;
        std::vector<struct epoll_event> events;
        FastQueue<Order, QUEUE_MASK, L1_CACHE_LINE> fastQueue;
        std::unique_ptr<OrderBook<>> book;
        std::stop_source ss;

        state(size_t size)
            : listener(get_listener_socket())
            , epollfd(-1)
            , events(size)
            , fastQueue(FastQueue<Order, QUEUE_MASK, L1_CACHE_LINE>())
            , book(std::make_unique<OrderBook<>>())
        {
        }
        void request_shutdown() { ss.request_stop(); }
};

int main()
{
        std::cin.tie(nullptr);
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);

        struct state gate(64);
        std::vector<int> connections { gate.listener };
        show_hostname();
        setnonblocking(gate.listener);

        if ((gate.epollfd = epoll_create1(0)) == -1) {
                std::cerr << "epoll_create1\n";
                std::exit(1);
        }

        struct epoll_event ev = { .events = EPOLLIN, .data = { .fd = gate.listener } };
        epoll_ctl(gate.epollfd, EPOLL_CTL_ADD, gate.listener, &ev);

        // thread 1 gotta loop
        std::jthread network_io_thread(
            [&](std::stop_token st) { network_io_worker(st, gate, connections); });

        std::jthread matching_engine_thread(
            [&](std::stop_token st) { matching_engine_worker(st, gate); });

        while (!gate.ss.stop_requested()) {
                if (g_stop_requested) {
                        gate.request_shutdown();
                } else {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
        }

        gate.fastQueue.stopQueue();
        close(gate.listener);
        return 0;
}

void network_io_worker(std::stop_token st, struct state& gate, std::vector<int>& connections)
{
        while (!st.stop_requested()) {
                run_server(gate.epollfd, gate.events, gate.listener, connections, gate.fastQueue);
        }
}

void matching_engine_worker(std::stop_token st, struct state& gate)
{
        while (!st.stop_requested()) {
                Order order = gate.fastQueue.pop();
                if (gate.fastQueue.isQueueStopped())
                        break;
                gate.book->add_order(order.is_buy, order.price, order.quantity);
                gate.book->status();
        }
}
