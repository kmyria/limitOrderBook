// gateway.cpp

#include "client.hpp"
#include "orderbook.hpp"
#include "ringbuffer.hpp"
#include "server.hpp"
#include <cstddef>
#include <iostream>
#include <memory>
#include <stop_token>
#include <sys/epoll.h>
#include <thread>
#include <vector>

void network_io_worker(std::stop_token st, struct state& gate, std::vector<int>& connections);
void matching_engine_worker(std::stop_token st, struct state& gate);

void signal_handler(std::stop_source& ss) { ss.request_stop(); }

struct state {
        int listener { };
        int epollfd;
        std::vector<struct epoll_event> events;
        std::unique_ptr<Ringbuffer<q_order, CAP>> ring_buf;
        std::unique_ptr<OrderBook> book;
        std::stop_source ss;

        state(size_t size)
            : listener(get_listener_socket())
            , epollfd(-1)
            , events(size)
            , ring_buf(std::make_unique<Ringbuffer<q_order, CAP>>())
            , book(std::make_unique<OrderBook>())
        {
        }
        void request_shutdown() { ss.request_stop(); }
};

int main()
{
        std::cin.tie(nullptr);

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
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        close(gate.listener);
        return 0;
}

void network_io_worker(std::stop_token st, struct state& gate, std::vector<int>& connections)
{
        while (!st.stop_requested()) {
                run_server(gate.epollfd, gate.events, gate.listener, connections, gate.ring_buf);
        }
}

void matching_engine_worker(std::stop_token st, struct state& gate)
{
        q_order order;
        while (!st.stop_requested()) {
                if (gate.ring_buf->pop(order)) {
                        gate.book->add_order(order.is_buy, order.price, order.quantity);
                        gate.book->status();
                }
        }
}
