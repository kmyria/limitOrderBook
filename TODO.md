# TODO

Optimization / correctness backlog for the matching engine + FastQueue pipeline.
Ordered roughly by "do correctness first, then perf". Baseline numbers and
measurement methodology live in [`BENCHMARK.md`](BENCHMARK.md).

## Correctness & features (before further perf work)

- [ ] **Price-ladder verifier / unit tests.** Assert price-time priority, FIFO
      within a price level, partial-fill accounting, and full-level removal.
      Cover the intentional fix: bids now match *highest-first* (the old vector
      book matched sells against the lowest bid).
- [ ] **Cancel / replace support.** Engine only handles `add_order` today.
      Add cancel/replace (IOC/GTC) semantics.
- [ ] **Order-id → resting-order index.** Needed for O(1) cancel/ack and
      market-by-price queries; currently `order_id` is assigned but has no
      lookup structure.

## Matching engine hot path

- [ ] **Kill steady-state allocations.** Each resting order allocates a
      `std::list` node (and a `std::map` node when a new price appears) on the
      consumer path. Replace with an intrusive order list backed by a
      per-consumer object pool / free list.
- [ ] **Evaluate a flat price ladder** (sorted active levels or direct
      tick-indexed buckets) vs `std::map` for cache locality / fewer indirections,
      keeping integer-cent prices.
- [ ] **Alignment / hot-cold split.** Check `Order` and price-level layout
      (currently `Order` is < 64 B so `FastQueue` pads the ring line); separate
      matching-critical fields from bookkeeping.
- [ ] **Gateway stdout spam.** `matching_engine_worker` calls `book->status()`
      and the engine prints every fill on **every order** outside the benchmark —
      real I/O bottleneck. Debug-gate or throttle it.
- [ ] Fill/cancel reporting to clients (currently nothing is sent back over the
      socket).

## Queue / producer side

- [ ] **Thread pinning + scheduling.** Gateway (and benchmark) threads are
      unpinned on a hybrid P/E-core CPU → p99/max outliers and ~±15% throughput
      scatter. Pin producers/consumers to physical cores (`SCHED_FIFO` where
      permitted). See `BENCHMARK.md` "Environment".
- [ ] **Idle behavior.** FastQueue `push`/`pop` are blocking spin loops; a
      producer with no messages burns a full core. Evaluate
      `tryPush`/`tryPop` + backoff or a sleep path when idle (trade CPU vs
      wake-up latency).
- [ ] **Re-validate FastQueue memory ordering.** The vendored queue uses
      `volatile` + `sfence`/`lfence` rather than C++ atomics; verify correctness
      claims before touching (third-party, MIT, Anders Cedronius).
- [ ] **Pick `QUEUE_MASK` from the latency budget.** Sweep data: flood
      p99 ≈ 92 µs @ 256 slots, ≈ 210 µs @ 512 slots; throughput is flat, so
      choose capacity from acceptable saturated worst-case residency / burst size.

## Benchmark harness (`src/bench/benchmark.cpp`)

- [ ] Pin benchmark threads for stable, reproducible numbers.
- [ ] Multi-iteration averaging + warmup to cut the ±15% scatter.
- [ ] `rdtsc` timestamps (fixed-TSC x86) to see sub-100 ns floors and drop the
      ~50 ns/order `steady_clock` overhead.
- [ ] Report fill rate (matched vs resting) so matching vs book-insert cost is
      interpretable.
- [ ] Optional end-to-end gateway benchmark (network → queue → book).

## Pipeline / process

- [ ] The server also needs a documented wire format for integer-cent prices
      (e.g. `1 10050 5`); currently only enforced implicitly by `parse_order`.
