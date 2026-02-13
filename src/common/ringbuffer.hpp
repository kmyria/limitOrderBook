// ringbuffer.cpp
#pragma once

#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <new>

#define CAP 1024

template <typename T, std::size_t N>

class Ringbuffer {
public:
    static_assert(std::has_single_bit(N), "Buffer size must be pow of 2");

    Ringbuffer()
        : head_ { 0 }
        , tail_(0) { };

    bool push(const T& val)
    {
        size_t h = head_.load(std::memory_order_relaxed);
        size_t t = tail_.load(std::memory_order_relaxed);

        std::size_t next = (h + 1) & (N - 1);
        if (next == t) {
            return false;
        }

        buffer[h] = val;
        head_.store(next, std::memory_order_release);

        return true;
    }

    bool pop(T& val)
    {
        size_t h = head_.load(std::memory_order_acquire);
        size_t t = tail_.load(std::memory_order_relaxed);

        std::size_t next = (t + 1) & (N - 1);
        if (h == t) {
            return false;
        }

        val = buffer[t];
        tail_.store(next, std::memory_order_release);

        return true;
    }

private:
    alignas(
        std::hardware_constructive_interference_size) std::atomic<size_t> head_;
    alignas(
        std::hardware_constructive_interference_size) std::atomic<size_t> tail_;

    std::array<T, N> buffer;
};
