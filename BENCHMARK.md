# Benchmark Report

Measurements taken with `src/bench/benchmark.cpp` against the SPSC `FastQueue`
producer/consumer pipeline feeding the matching engine's `OrderBook`.

## TL;DR

| metric | before (vector book) | after (price ladder) |
|---|---|---|
| flood throughput | ~20.7k orders/s | ~2.9–3.9M orders/s |
| true per-order latency (paced, 1 in flight) | not measurable | p50 ≈ 470 ns, p99 ≈ 946 ns |
| flood latency @ default ring | ~49 ms avg | ~0.3 ms avg (= ring backlog, see below) |

The ~150x throughput jump came from replacing the naive sorted-`std::vector`
book (O(resting orders) memmoves per insert/erase) with a price ladder
(`std::map<uint32_t, std::list<Order>>`, price-time priority in integer cents).

## Reproduce

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

./build/benchmark 1000000          # flood  (saturated pipeline, default ring)
./build/benchmark 1000000 paced    # latency with 1 order in flight (no backlog)
./build/benchmark 1000000 sweep    # flood across ring sizes 2^2..2^12
```

## What each mode measures

- **Flood** — producer pushes as fast as it can; the ring saturates (~90% full).
  Reported latency is dominated by *queue residency* (waiting in line), so it is
  a saturation/backlog metric, not per-order cost. Throughput is consumer-bound.
- **Paced** — producer waits until the previous order is fully matched before
  sending the next. The queue never backs up, so latency ≈ true
  push → pop → match per-order time.
- **Sweep** — flood repeated at ring sizes `2^2..2^12` (usable slots `3..4095`)
  to show the latency/capacity relationship.

Both modes use the same RNG seed, so order streams are identical across runs.

## Environment

- Linux, C++23, Release (`-DCMAKE_BUILD_TYPE=Release`), `-Wall -Wextra -Wpedantic`
- Hybrid P/E-core CPU; benchmark threads are **not pinned** and run at normal
  scheduler priority → run-to-run scatter (~±15%) and rare ms-level `max`
  outliers are OS scheduling noise, not queue behavior.

## Results

### 1. Baseline: naive vector book (before the price-ladder rewrite)

Sorted `std::vector<Order>` per side; `insert`/`erase` shift the whole tail →
O(N^2) overall.

| orders | elapsed | throughput | min | avg | p50 | p90 | p99 | max |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1,000,000 | 48.40 s | 20,660 /s | 55.6 µs | 49.5 ms | 38.6 ms | 103.2 ms | 177.4 ms | 270.8 ms |

### 2. After price-ladder fix — flood (default ring, `QUEUE_MASK = 1023`)

Two independent runs showing normal variance.

| run | elapsed | throughput | min | avg | p50 | p90 | p99 | max |
|---|---|---|---|---|---|---|---|---|
| A | 307.6 ms | 3,250,525 /s | 103.6 µs | 286.5 µs | 281.1 µs | 300.4 µs | 378.2 µs | 618 µs |
| B | 339.3 ms | 2,947,055 /s | 0.8 µs | 308.6 µs | 294.1 µs | 349.4 µs | 531.8 µs | 2.05 ms |

`min` reflects the brief unsaturated startup before the ring fills; `avg/p50/p99`
describe the saturated steady state.

### 3. Paced latency (1 order in flight, default ring)

| orders | elapsed | min | avg | p50 | p90 | p99 | max | implied serial |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 1,000,000 | 714.5 ms | 161 ns | 501 ns | 470 ns | 646 ns | 946 ns | 1.59 ms | ~2.0M /s |

Each sample includes two `steady_clock` reads (~50 ns total), so the floor is
clock-bound; the engine handoff + match itself is roughly 400–450 ns. The `max`
(1.59 ms) is a single OS-noise outlier.

### 4. Ring-size sweep (flood)

| slots | orders/s | avg | p50 | p99 | max |
|---:|---:|---:|---:|---:|---:|
| 4 | 2,923,203 | 1.2 µs | 1.1 µs | 4.4 µs | 630 µs |
| 8 | 3,148,808 | 2.3 µs | 2.2 µs | 4.4 µs | 1.93 ms |
| 16 | 3,428,387 | 4.3 µs | 4.1 µs | 7.0 µs | 124 µs |
| 32 | 2,771,831 | 9.6 µs | 8.8 µs | 17.6 µs | 107 µs |
| 64 | 3,162,332 | 16.2 µs | 15.7 µs | 29.2 µs | 1.84 ms |
| 128 | 3,172,769 | 33.2 µs | 32.4 µs | 57.7 µs | 131 µs |
| 256 | 3,869,362 | 57.1 µs | 53.9 µs | 92.2 µs | 2.19 ms |
| 512 | 3,464,937 | 132.5 µs | 134.5 µs | 210.6 µs | 3.39 ms |
| 1024 | 3,870,387 | 239.2 µs | 236.9 µs | 393.9 µs | 3.68 ms |
| 2048 | 3,685,728 | 494.1 µs | 519.0 µs | 791.9 µs | 880 µs |
| 4096 | 3,230,234 | 1.14 ms | 1.13 ms | 1.50 ms | 1.69 ms |

## Analysis

- **Throughput is capacity-independent** (~2.8–3.9M/s across all sizes): the
  consumer (matching) is the bottleneck, not the queue.
- **Flood latency ≈ 0.9 × ring_slots × service_time.** Under a saturated flood
  the producer/consumer hold the ring ~90% full, so avg latency grows linearly
  with capacity (1.2 µs at 4 slots → 1.14 ms at 4096 slots). This is backlog
  wait, not per-order cost — see paced (row 3).
- **Choose `QUEUE_MASK` from a latency/burst budget**, not throughput: e.g.
  p99 ≈ 92 µs at 256 slots, ≈ 210 µs at 512 slots. Larger rings absorb bursts
  better but impose deeper worst-case residency when saturated.

See `TODO.md` for the optimization backlog this data motivates.
