# Low-Latency Limit Order Book & Matching Engine

High performance multi-threaded exchange simulator written in modern C++. Designed to simulate architecture of real-world trading system, focusing on deterministic latency, cache locality, and lock-free concurrency.

## Architecture

The system follows a classic producer-consumer architecture separated into two pinned threads to minimize context switching:

1.  **Network gateway (Producer):**
    *   Utilizes epoll for O(1) I/O multiplexing.
    *   Handles TCP client connections and raw binary protocol parsing.
    *   Non-blocking I/O loop designed to prevent thread starvation.

2.  **Matching engine (Consumer):**
    *   Implements price-time priority matching algorithm.
    *   Utilizes contiguous memory containers for the orderbook to maximize L1/L2 cache hits, preferring linear scans over pointer chasing (`std::map`) for small N depth.
    *   Deterministic execution path with zero dynamic allocation in the hot path.

3.  **Inter-thread communication:**
    *   Connected via a custom single-producer single-consumer (SPSC) lock-free ring buffer.
    *   Uses memory ordering constraints (`acquire`/`release`) to enforce synchronization without mutexes or kernel-level locking overhead.


## Build & Run

**Requirements:**
*   c++23 compliant compiler
*   cmake 3.16

**Compilation:**
```bash
cmake -B build
cmake --build build
```

**Usage:**
```bash
cd build/
./build/gateway
```

The server listens on port 1337. Clients can connect via Telnet or `./build/client`.
