#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <memory>
#include <new>
#include <stop_token>
#include <thread>

template <typename T, std::size_t N>

class Ringbuffer {
public:
    Ringbuffer()
        : head_ { 0 }
        , tail_(0) { };

    bool push(const T& val)
    {
        size_t h = head_.load();
        size_t t = tail_.load();

        std::size_t next = (h + 1) & (N - 1);
        if (next == t) {
            return false;
        }

        buffer[h] = val;
        head_.store(next);

        return true;
    }

    bool pop(T& val)
    {
        size_t h = head_.load();
        size_t t = tail_.load();

        if (h == t) {
            return false;
        }

        val = buffer[t];
        tail_.store((t + 1) & (N - 1));

        return true;
    }

private:
    alignas(
        std::hardware_constructive_interference_size) std::atomic<size_t> head_;
    alignas(
        std::hardware_constructive_interference_size) std::atomic<size_t> tail_;

    std::array<T, N> buffer;
};

void producer_worker(std::stop_token st, std::stop_source& ss,
    std::unique_ptr<Ringbuffer<int, 1024>>& spsc)
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
    std::unique_ptr<Ringbuffer<int, 1024>>& spsc)
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
    std::unique_ptr<Ringbuffer<int, 1024>> spsc
        = std::make_unique<Ringbuffer<int, 1024>>();

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
