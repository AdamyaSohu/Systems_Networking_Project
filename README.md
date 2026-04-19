# Cache-Aware Packet Buffer Simulator

Two C++ projects that form a single coherent argument about memory hierarchy and systems performance.

The first project is a configurable multi-level cache simulator built from scratch. The second is a cycle-accurate network packet buffer simulator that uses the cache simulator as its timing model — demonstrating that ring buffer working sets exceeding L1 cache capacity cause throughput degradation and non-zero packet drop rates at moderate arrival rates.

---

## Repository Layout

```
systems_networking_project/
├── cache_sim/
│   ├── CacheLine.h
│   ├── ReplacementPolicy.h
│   ├── Cache.h
│   ├── Cache.cpp
│   ├── CacheHierarchy.h
│   ├── CacheHierarchy.cpp
│   ├── TraceGenerator.cpp
│   └── main.cpp
└── packet_sim/
    ├── Packet.h
    ├── RingBuffer.h
    ├── RingBuffer.cpp
    ├── PacketProcessor.h
    ├── PacketProcessor.cpp
    ├── PacketGenerator.cpp
    └── main.cpp
```

---

## Part 1: Cache Simulator

### Overview

A configurable set-associative cache simulator modeling a three-level memory hierarchy (L1 / L2 / L3). The simulator accepts any combination of set count, associativity, block size, and replacement policy at each level, and reports hit rates and Average Memory Access Time (AMAT) across synthetic access traces.

### Design

A memory address is decomposed into three fields — tag, set index, and block offset — using bit manipulation. Each access is classified as a hit or miss by comparing the address tag against all valid lines in the target set.

Three replacement policies share a common polymorphic interface:

- **LRU** — evicts the line accessed furthest in the past
- **FIFO** — evicts the line that has been resident longest, regardless of recency
- **Random** — selects a victim uniformly at random

The three-level hierarchy passes misses downward: L1 miss → L2 → L3 → RAM. AMAT is computed as:

```
AMAT = T1 + MR1 × (T2 + MR2 × (T3 + MR3 × Tmem))
```

Default latencies: L1 = 4 cycles, L2 = 12 cycles, L3 = 40 cycles, RAM = 200 cycles.

### Cache Parameters (defaults)

| Level | Sets | Ways | Block Size | Capacity |
|-------|------|------|------------|----------|
| L1    | 64   | 8    | 64 B       | 32 KB    |
| L2    | 512  | 8    | 64 B       | 256 KB   |
| L3    | 8192 | 16   | 64 B       | 8 MB     |

### Access Patterns

Five synthetic traces isolate specific cache behaviors:

- **Sequential** — marches through memory one block at a time, never revisiting an address. Worst case for any cache.
- **Strided** — jumps by a fixed stride between accesses, eliminating spatial locality.
- **Random** — draws addresses uniformly from a bounded region. No locality of any kind.
- **Working set** — repeatedly cycles through a fixed set of blocks. Two configurations: one that fits in L1, one that overflows by four blocks. This boundary is the central experimental variable.
- **Mixed** — 80% sequential, 20% random. Models a workload with dominant but imperfect locality.

### Key Results

| Pattern | Policy | L1 Hit Rate | AMAT (cycles) |
|---------|--------|-------------|---------------|
| Sequential | LRU | 0% | 256.00 |
| Working set (fits L1) | LRU | 99.92% | 4.20 |
| Working set (overflows L1) | LRU | 0% | 16.29 |
| Working set (overflows L1) | Random | 33.41% | 12.28 |
| Random | LRU | 0.13% | 249.90 |

**Access pattern dominates policy.** For sequential, strided, and random traces, all three policies produce identical or near-identical results. Policy only matters when the cache is full and competing blocks must share limited space.

**The working set boundary is the most important parameter.** A working set that fits in L1 produces a 4.20-cycle AMAT — 61× lower than sequential access. Overflowing L1 by just four blocks raises AMAT to 16.29 cycles under LRU.

**Random eviction outperforms LRU on cyclic overflow.** When the working set of 12 blocks cycles through an 8-block L1, LRU's deterministic eviction pattern aligns pathologically with the access pattern, achieving 0% L1 hits. Random eviction achieves 33.4% hits and reduces AMAT by 25%.

### Build and Run

```bash
# Build the cache simulator
g++ -std=c++20 -Wall -o cache_sim \
    cache_sim/Cache.cpp \
    cache_sim/CacheHierarchy.cpp \
    cache_sim/main.cpp

./cache_sim
```

---

## Part 2: Packet Buffer Simulator

### Overview

A cycle-accurate network packet buffer simulator that models the impact of ring buffer memory layout on packet processing throughput. The cache simulator serves as the timing engine — every packet byte access is routed through the cache hierarchy, and the simulation clock advances by the actual cache access cost rather than a flat latency.

### Design

Four components:

- **Packet** — a struct with sequence number, arrival timestamp, size field, and a fixed payload array up to 1,536 bytes (Ethernet MTU).
- **RingBuffer** — a fixed-capacity circular queue of Packet objects. Tracks arrivals, drops, latency, and throughput.
- **PacketProcessor** — dequeues one packet at a time and walks through it 64 bytes at a time, issuing one cache access per block. The simulation clock advances by the cache cost of each access (4 / 12 / 40 / 200 cycles depending on which level is hit).
- **PacketGenerator** — writes trace files of packet arrivals to disk. The simulator reads these traces and replays them.

The key design decision: each ring buffer slot maps to a fixed memory region (slot address = `(sequenceNumber % bufferCapacity) × sizeof(Packet)`). The processor repeatedly accesses the same physical locations as it cycles through the buffer. The working set is exactly the ring buffer's memory footprint.

### Buffer Configurations

Four sizes straddle the cache level boundaries:

| Configuration | Slots | Footprint  | Working Set Location |
|---------------|-------|------------|----------------------|
| Small         | 8     | 12,480 B   | L1                   |
| Medium        | 100   | 156,000 B  | L2                   |
| Large         | 1,000 | 1,560,000 B| L3                   |
| Very large    | 6,000 | 9,360,000 B| RAM                  |

### Traffic Patterns

- **Constant rate** — one packet every 1,000 cycles. Relaxed enough that no buffer is capacity-stressed. Isolates the cache effect from queuing pressure.
- **Bursty** — 8 packets arrive simultaneously every 1,000 cycles. Stresses buffer capacity — small buffers fill instantly and drop packets; large buffers absorb bursts but drain them slowly.
- **Mixed size** — packets alternate between 64-byte and 1,536-byte. Small packets touch one cache line; large packets touch 24. At the same arrival rate, large packets drive cache thrashing that does not appear in the constant trace.

### Key Results (1,000,000 packets)

**Bursty trace:**

| Buffer | Footprint   | Fits in | Dropped | Drop Rate | Throughput        | AMAT      |
|--------|-------------|---------|---------|-----------|-------------------|-----------|
| 8      | 12,480 B    | L1      | 125,002 | 12.50%    | 0.0070 pkts/cycle | 4.00 cyc  |
| 100    | 156,000 B   | L2      | 0       | 0.00%     | 0.0080 pkts/cycle | 4.03 cyc  |
| 1,000  | 1,560,000 B | L3      | 0       | 0.00%     | 0.0080 pkts/cycle | 16.24 cyc |
| 6,000  | 9,360,000 B | RAM     | 0       | 0.00%     | 0.0080 pkts/cycle | 43.66 cyc |

**Mixed size trace:**

| Buffer | Footprint   | Fits in | Dropped | Drop Rate | Avg Latency       | AMAT      |
|--------|-------------|---------|---------|-----------|-------------------|-----------|
| 8      | 12,480 B    | L1      | 6       | 0.0006%   | 46 cyc/pkt        | 4.00 cyc  |
| 100    | 156,000 B   | L2      | 52      | 0.0052%   | 156 cyc/pkt       | 16.02 cyc |
| 1,000  | 1,560,000 B | L3      | 502     | 0.0502%   | 2,627 cyc/pkt     | 56.20 cyc |
| 6,000  | 9,360,000 B | RAM     | 3,002   | 0.3002%   | 78,693 cyc/pkt    | 57.20 cyc |

**A larger buffer does not guarantee lower drop rates.** On the mixed trace, the 8-slot L1 buffer drops 6 packets while the 6,000-slot RAM buffer drops 3,002 — because the large buffer's working set spills entirely into RAM, slowing the processor below the arrival rate. The smallest buffer, staying entirely in L1, processes packets fast enough to keep up.

**Cache boundaries are sharp, not gradual.** Moving the working set from L2 into L3 multiplies AMAT by roughly 4×. Moving from L3 into RAM multiplies it by another 3×. The hierarchy does not degrade gracefully — it degrades at discrete thresholds.

**Latency is a more sensitive indicator than drop rate.** On the bursty trace, buffers larger than 8 slots report zero drops — but latency climbs from 15 cycles (L2) to 105 cycles (L3) to 2,539 cycles (RAM). Drop rate alone understates the performance cost of cache pressure.

### Build and Run

```bash
# Build the packet generator (standalone binary)
g++ -std=c++20 -Wall -o packetgen \
    packet_sim/PacketGenerator.cpp

# Build the packet simulator (links against cache simulator)
g++ -std=c++20 -Wall -o packet_sim_run \
    packet_sim/PacketProcessor.cpp \
    packet_sim/RingBuffer.cpp \
    cache_sim/Cache.cpp \
    cache_sim/CacheHierarchy.cpp \
    packet_sim/main.cpp

# Generate traces
./packetgen constant 1000000 1000 trace_constant.txt
./packetgen bursty   1000000 1000 trace_bursty.txt
./packetgen mixed    1000000 1000 trace_mixed.txt

# Run experiments (four buffer sizes, automatically)
./packet_sim_run trace_constant.txt
./packet_sim_run trace_bursty.txt
./packet_sim_run trace_mixed.txt
```

The simulator runs all four buffer configurations automatically for each trace and prints results to stdout.

---

## Requirements

- C++20 compiler (clang++ or g++)
- No external dependencies
