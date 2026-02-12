// ringbuffer.cpp

#include "ringbuffer.hpp"

/*
void producer_worker(std::stop_token st, std::stop_source& ss,
    std::unique_ptr<Ringbuffer<int, CAP>>& spsc)
{
    int i {};
    while (!st.stop_requested()) {
        if (++i == INT_MAX) {
            ss.request_stop();
            return;
        }
        while (!spsc->push(i))
            ;
    }
}

void consumer_worker(std::stop_token st, std::stop_source& ss,
    std::unique_ptr<Ringbuffer<int, CAP>>& spsc)
{
    int s;
    while (!st.stop_requested()) {
        if (spsc->pop(s)) {
            std::cout << s << '\n';
        }
    }
}

int main()
{
    std::unique_ptr<Ringbuffer<int, CAP>> spsc
        = std::make_unique<Ringbuffer<int, CAP>>();

    std::stop_source ss;
    std::jthread producer_thread(
        [&](std::stop_token st) { producer_worker(st, ss, spsc); });
    std::jthread consumer_thread(
        [&](std::stop_token st) { consumer_worker(st, ss, spsc); });

    while (!ss.stop_requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return 0;
}
*/
