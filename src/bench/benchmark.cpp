// benchmark.cpp

#include "fastqueue.hpp"
#include "orderbook.hpp"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

using OrderQueue = FastQueue<Order, QUEUE_MASK, L1_CACHE_LINE>;

uint64_t now_ns()
{
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());
}

template <typename Queue>
void producer(Queue& queue, std::vector<uint64_t>& t0, std::atomic<bool>& go, size_t total)
{
        while (!go.load(std::memory_order_acquire)) {
        }

        std::mt19937_64 rng(0x5EED);
        std::uniform_int_distribution<uint32_t> side(0, 1);
        std::uniform_int_distribution<uint32_t> cents(100, 20000);
        std::uniform_int_distribution<uint32_t> qty(1, 100);

        for (size_t seq = 0; seq < total; ++seq) {
                Order order {
                    .id = seq,
                    .is_buy = side(rng) == 0,
                    .price = cents(rng),
                    .quantity = qty(rng),
                };
                t0[seq] = now_ns();
                queue.push(order);
        }
        queue.stopQueue();
}

template <typename Queue>
void consumer(Queue& queue, std::vector<uint64_t>& t1, std::atomic<bool>& go, size_t total,
    std::atomic<size_t>& done)
{
        while (!go.load(std::memory_order_acquire)) {
        }

        OrderBook<false> book;
        size_t processed = 0;
        while (processed < total) {
                Order order = queue.pop();
                if (order.quantity == 0)
                        break;
                book.add_order(order.is_buy, order.price, order.quantity);
                t1[order.id] = now_ns();
                ++processed;
        }
        done.store(processed, std::memory_order_release);
}

template <typename Queue>
void paced_producer(Queue& queue, std::vector<uint64_t>& t0, std::atomic<bool>& go,
    std::atomic<size_t>& done, size_t total)
{
        while (!go.load(std::memory_order_acquire)) {
        }

        std::mt19937_64 rng(0x5EED);
        std::uniform_int_distribution<uint32_t> side(0, 1);
        std::uniform_int_distribution<uint32_t> cents(100, 20000);
        std::uniform_int_distribution<uint32_t> qty(1, 100);

        for (size_t seq = 0; seq < total; ++seq) {
                while (done.load(std::memory_order_acquire) < seq) {
                }
                Order order {
                    .id = seq,
                    .is_buy = side(rng) == 0,
                    .price = cents(rng),
                    .quantity = qty(rng),
                };
                t0[seq] = now_ns();
                queue.push(order);
        }
        queue.stopQueue();
}

template <typename Queue>
void paced_consumer(Queue& queue, std::vector<uint64_t>& t1, std::atomic<bool>& go, size_t total,
    std::atomic<size_t>& done)
{
        while (!go.load(std::memory_order_acquire)) {
        }

        OrderBook<false> book;
        size_t processed = 0;
        while (processed < total) {
                Order order = queue.pop();
                if (order.quantity == 0)
                        break;
                book.add_order(order.is_buy, order.price, order.quantity);
                t1[order.id] = now_ns();
                ++processed;
                done.store(processed, std::memory_order_release);
        }
}

double percentile(const std::vector<uint64_t>& v, double p)
{
        if (v.empty())
                return 0.0;
        double pos = p * static_cast<double>(v.size() - 1);
        size_t lo = static_cast<size_t>(pos);
        size_t hi = std::min(lo + 1, v.size() - 1);
        double frac = pos - static_cast<double>(lo);
        return static_cast<double>(v[lo]) * (1.0 - frac) + static_cast<double>(v[hi]) * frac;
}

struct Stats {
        double min;
        double avg;
        double p50;
        double p90;
        double p99;
        double max;
};

Stats make_stats(const std::vector<uint64_t>& sorted)
{
        double sum = 0.0;
        for (uint64_t ns : sorted) {
                sum += static_cast<double>(ns);
        }
        Stats s;
        s.min = static_cast<double>(sorted.front());
        s.max = static_cast<double>(sorted.back());
        s.avg = sum / static_cast<double>(sorted.size());
        s.p50 = percentile(sorted, 0.50);
        s.p90 = percentile(sorted, 0.90);
        s.p99 = percentile(sorted, 0.99);
        return s;
}

void print_detail(const Stats& s)
{
        std::cout << std::fixed << std::setprecision(1);
        std::cout << std::setw(6) << "min" << ": " << s.min << " ns (" << s.min / 1000.0
                  << " us)\n";
        std::cout << std::setw(6) << "avg" << ": " << s.avg << " ns (" << s.avg / 1000.0 << " us)\n";
        std::cout << std::setw(6) << "p50" << ": " << s.p50 << " ns (" << s.p50 / 1000.0 << " us)\n";
        std::cout << std::setw(6) << "p90" << ": " << s.p90 << " ns (" << s.p90 / 1000.0 << " us)\n";
        std::cout << std::setw(6) << "p99" << ": " << s.p99 << " ns (" << s.p99 / 1000.0 << " us)\n";
        std::cout << std::setw(6) << "max" << ": " << s.max << " ns (" << s.max / 1000.0
                  << " us)\n";
}

struct RunResult {
        Stats stats;
        uint64_t elapsed_ns;
        double throughput;
};

template <uint64_t Mask>
RunResult run_flood(size_t total)
{
        FastQueue<Order, Mask, L1_CACHE_LINE> queue;
        std::vector<uint64_t> t0(total, 0);
        std::vector<uint64_t> t1(total, 0);
        std::atomic<bool> go { false };
        std::atomic<size_t> done { 0 };

        std::thread p([&] { producer(queue, t0, go, total); });
        std::thread c([&] { consumer(queue, t1, go, total, done); });

        uint64_t start = now_ns();
        go.store(true, std::memory_order_release);

        p.join();
        c.join();
        uint64_t elapsed_ns = now_ns() - start;

        if (done.load(std::memory_order_acquire) != total) {
                std::cerr << "consumer processed " << done.load() << " of " << total
                          << " orders\n";
                std::exit(1);
        }

        std::vector<uint64_t> lat(total, 0);
        for (size_t i = 0; i < total; ++i) {
                lat[i] = t1[i] - t0[i];
        }
        std::sort(lat.begin(), lat.end());

        RunResult r;
        r.stats = make_stats(lat);
        r.elapsed_ns = elapsed_ns;
        r.throughput = static_cast<double>(total) / (static_cast<double>(elapsed_ns) / 1e9);
        return r;
}

RunResult run_paced(size_t total)
{
        OrderQueue queue;
        std::vector<uint64_t> t0(total, 0);
        std::vector<uint64_t> t1(total, 0);
        std::atomic<bool> go { false };
        std::atomic<size_t> done { 0 };

        std::thread p([&] { paced_producer(queue, t0, go, done, total); });
        std::thread c([&] { paced_consumer(queue, t1, go, total, done); });

        uint64_t start = now_ns();
        go.store(true, std::memory_order_release);

        p.join();
        c.join();
        uint64_t elapsed_ns = now_ns() - start;

        if (done.load(std::memory_order_acquire) != total) {
                std::cerr << "consumer processed " << done.load() << " of " << total
                          << " orders\n";
                std::exit(1);
        }

        std::vector<uint64_t> lat(total, 0);
        for (size_t i = 0; i < total; ++i) {
                lat[i] = t1[i] - t0[i];
        }
        std::sort(lat.begin(), lat.end());

        RunResult r;
        r.stats = make_stats(lat);
        r.elapsed_ns = elapsed_ns;
        r.throughput = static_cast<double>(total) / (static_cast<double>(elapsed_ns) / 1e9);
        return r;
}

void report_flood(size_t total, const RunResult& r)
{
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "orders:           " << total << "\n";
        std::cout << "elapsed:          " << static_cast<double>(r.elapsed_ns) / 1000.0 << " us\n";
        std::cout << "throughput:       " << r.throughput << " orders/s\n";
        std::cout << "pipeline latency (queue residency + match):\n";
        print_detail(r.stats);
}

void report_paced(size_t total, const RunResult& r)
{
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "orders:           " << total << "\n";
        std::cout << "elapsed:          " << static_cast<double>(r.elapsed_ns) / 1000.0 << " us\n";
        std::cout << "paced latency (1 order in flight, no queue backlog):\n";
        print_detail(r.stats);
        std::cout << std::setprecision(3)
                  << "implied serial throughput (1 / avg latency): "
                  << 1e9 / r.stats.avg << " orders/s\n";
        std::cout << "note: each sample includes two steady_clock reads (~50 ns);\n"
                     "the floor is clock-bound, not queue-bound.\n";
}

template <uint64_t Mask>
void sweep_row(size_t total)
{
        RunResult r = run_flood<Mask>(total);
        std::cout << std::fixed << std::setprecision(0);
        std::cout << std::setw(9) << (Mask + 1) << std::setw(14) << r.throughput
                  << std::setw(13) << r.stats.avg << std::setw(13) << r.stats.p50
                  << std::setw(13) << r.stats.p99 << std::setw(13) << r.stats.max << "\n";
}

template <size_t... Ks>
void run_sweep(size_t total, std::index_sequence<Ks...>)
{
        std::cout << std::setw(9) << "slots"
                  << std::setw(14) << "orders/s"
                  << std::setw(13) << "avg_ns"
                  << std::setw(13) << "p50_ns"
                  << std::setw(13) << "p99_ns"
                  << std::setw(13) << "max_ns\n";
        (sweep_row<(1ull << (Ks + 2)) - 1>(total), ...);
}

void usage(const char* prog)
{
        std::cerr << "usage: " << prog << " [num_orders] [flood|paced|sweep]\n";
}

int main(int argc, char** argv)
{
        size_t total = 1000000;
        std::string_view mode = "flood";

        if (argc > 1) {
                std::string_view arg(argv[1]);
                size_t parsed = 0;
                auto [ptr, ec] = std::from_chars(arg.data(), arg.data() + arg.size(), parsed);
                if (ec != std::errc() || ptr != arg.data() + arg.size() || parsed == 0) {
                        usage(argv[0]);
                        return 1;
                }
                total = parsed;
        }
        if (argc > 2) {
                mode = argv[2];
                if (mode != "flood" && mode != "paced" && mode != "sweep") {
                        usage(argv[0]);
                        return 1;
                }
        }

        if (mode == "flood") {
                RunResult r = run_flood<QUEUE_MASK>(total);
                report_flood(total, r);
        } else if (mode == "paced") {
                RunResult r = run_paced(total);
                report_paced(total, r);
        } else {
                run_sweep(total, std::make_index_sequence<11>());
        }

        return 0;
}
