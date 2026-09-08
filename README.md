# Low-Latency Limit Order Book & Matching Engine

High performance multi-threaded exchange simulator written in modern C++. Designed to simulate architecture of real-world trading system, focusing on deterministic latency, cache locality, and lock-free concurrency.

## Architecture

The system follows a classic producer-consumer architecture separated into two threads to minimize context switching:

1.  **Network gateway (Producer):**
    *   Utilizes epoll for O(1) I/O multiplexing.
    *   Handles TCP client connections and raw binary protocol parsing.
    *   Non-blocking I/O loop designed to prevent thread starvation.

2.  **Matching engine (Consumer):**
    *   Implements price-time priority matching on a **price ladder**: each side is a `std::map<uint32_t, std::list<Order>>` keyed by price (integer cents), with FIFO per price level. Partial fills decrement in place; empty levels are removed.
    *   Zero dynamic allocation in the hot path is a goal, not yet met — resting orders allocate per-order nodes. See `TODO.md`.

3.  **Inter-thread communication:**
    *   A single-producer single-consumer `FastQueue` (lock-free, cache-line padded) connects the gateway to the matching engine.
    *   Blocking `push`/`pop` spin loops with `stopQueue()` for clean shutdown.

4.  **Thread pinning** (real-time scheduling, core affinity) is planned but not yet implemented — see `TODO.md`.

Prices are handled as **integer cents** end-to-end (no floating point on the price path); the wire format is `is_buy price_cents quantity`, e.g. `1 10050 5`.

## Build & Run

**Requirements:**
*   c++23 compliant compiler
*   cmake 3.16

**Compilation (Release is required for meaningful performance numbers):**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

**Usage:**
```bash
./build/gateway            # server on port 1337
./build/client             # interactive client
./build/benchmark 1000000          # flood throughput
./build/benchmark 1000000 paced    # single-order latency
./build/benchmark 1000000 sweep    # ring-size sweep
```

The server listens on port 1337. Clients can connect via Telnet or `./build/client`.

## Docs

*   [`BENCHMARK.md`](BENCHMARK.md) — measurements and analysis of the FastQueue → matching-engine pipeline.
*   [`TODO.md`](TODO.md) — correctness and optimization backlog.
