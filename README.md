# Cache-Aware Packet Buffer Simulator

[![CI](https://github.com/AdamyaSohu/Systems_Networking_Project/actions/workflows/ci.yml/badge.svg)](https://github.com/AdamyaSohu/Systems_Networking_Project/actions/workflows/ci.yml)

Two C++20 simulators that form one argument about memory hierarchy and systems performance.

The first is a configurable multi-level cache simulator. The second is a network packet buffer simulator that uses the cache simulator as its timing model: every byte of every packet is charged the cost of a real cache access. Together they demonstrate that a ring buffer's memory footprint, not its slot count, determines whether a packet pipeline keeps up with its arrival rate. The headline result: on the same traffic, an 8-slot buffer whose working set fits in L1 drops 9 packets in a million, while a 6,000-slot buffer whose working set spills past L3 drops 8,223 and holds packets 3,600 times longer.

Every number in this README is regenerated from committed code by `ctest`; the key results are asserted in CI, so the tables cannot silently drift from the implementation.

## Repository layout

```
.
├── cache_sim/          set-associative cache model + trace generator
│   ├── Cache.{h,cpp}           one cache level
│   ├── CacheHierarchy.{h,cpp}  L1/L2/L3 composition, AMAT, latencies
│   ├── CacheLine.h             line + stats structs
│   ├── ReplacementPolicy.h     LRU / FIFO / Random strategies
│   ├── TraceGenerator.cpp      -> tracegen (memory access traces)
│   └── main.cpp                -> cache_sim (experiment driver)
├── packet_sim/         packet pipeline built on the cache model
│   ├── Packet.h                1,560-byte slot payload
│   ├── RingBuffer.{h,cpp}      fixed-capacity FIFO of packets
│   ├── PacketProcessor.{h,cpp} drains buffer, one cache block per tick
│   ├── PacketGenerator.cpp     -> packetgen (arrival traces)
│   └── main.cpp                -> packet_sim (experiment driver)
├── tests/              Catch2 unit tests + golden-number integration tests
├── results/            captured output of the exact runs quoted below
└── CMakeLists.txt
```

## Architecture

```
 packetgen ──▶ arrival trace (cycle, size)
                       │
                       ▼
              ┌─────────────────┐   enqueue on arrival
              │    main loop    │──────────────┐
              │  (sim clock)    │              ▼
              └────────┬────────┘      ┌───────────────┐
                       │ tick          │  RingBuffer   │  N slots x 1,560 B
                       ▼               └───────┬───────┘
              ┌─────────────────┐  dequeue     │
              │ PacketProcessor │◀─(slot idx)──┘
              └────────┬────────┘
                       │ one 64 B block per tick,
                       │ at the slot's memory address
                       ▼
              ┌─────────────────────────┐
              │      CacheHierarchy     │
              │  L1 ─▶ L2 ─▶ L3 ─▶ RAM  │
              │   4    12    40    200  │  (cycles)
              └─────────────────────────┘
```

The coupling is the point: the ring buffer's slot array is the processor's working set. The processor walks each dequeued packet 64 bytes at a time, issuing one hierarchy access per block at the address of the slot the packet actually occupied, and the simulation clock advances by whatever that access cost. Buffer sizing therefore moves the working set across cache level boundaries, and the effect on drops and latency is measured directly.

## Part 1: cache simulator

A set-associative cache is parameterized by set count, associativity, and block size. Addresses decompose into tag, set index, and block offset; a hit compares the tag against every valid line in the target set. Three replacement policies share a strategy interface and differ only in victim selection:

| Policy | Victim | Mechanism |
|--------|--------|-----------|
| LRU    | least recently touched | per-way timestamp, refreshed on hit and fill |
| FIFO   | oldest fill | per-way timestamp, refreshed on fill only |
| Random | uniform pick | per-set seeded `minstd_rand` |

LRU and FIFO are the same "evict the smallest timestamp" loop; the only difference is whether a hit refreshes the stamp. Implementing them as two subclasses of one timestamp policy makes that relationship explicit in the type system rather than buried in two unrelated data structures.

A three-level hierarchy composes three `Cache` instances. A miss at level N is presented to level N+1, every level a request passes through fills the block, and AMAT is computed from local miss rates:

```
AMAT = T1 + MR1 x (T2 + MR2 x (T3 + MR3 x Tram))
```

with default latencies of 4 / 12 / 40 / 200 cycles.

### Two geometries, and why the experiments use the small one

| Config | L1 | L2 | L3 |
|--------|----|----|----|
| `small` (default) | 4 sets x 2 ways x 16 B = 128 B | 8 x 4 x 16 = 512 B | 16 x 8 x 16 = 2 KB |
| `realistic` | 64 x 8 x 64 = 32 KB | 512 x 8 x 64 = 256 KB | 8192 x 16 x 64 = 8 MB |

The cache experiments run on 10,000-access traces. A 32 KB L1 never experiences capacity pressure at that scale, and capacity pressure is the object of study, so the experiment geometry is scaled down until a 10 K-access trace can overflow it. The packet simulator, whose traces run to a million packets, uses the realistic geometry. `cache_sim --config realistic` switches at the command line.

### Results (10,000 accesses per trace, seed 42)

Full output for every pattern and policy combination is in `results/results_cache.txt`. The rows that carry the findings:

| Pattern | Policy | L1 hit rate | AMAT (cycles) |
|---------|--------|-------------|---------------|
| Sequential | any | 0% | 256.00 |
| Working set, fits L1 (8 blocks) | any | 99.92% | 4.20 |
| Working set, overflows L1 (12 blocks) | LRU | 0% | 16.29 |
| Working set, overflows L1 (12 blocks) | FIFO | 0% | 16.29 |
| Working set, overflows L1 (12 blocks) | Random | 33.24% | 12.30 |
| Random | LRU | 0.22% | 249.61 |

Three findings. First, access pattern dominates policy: on sequential, strided, and random traces all three policies are within noise of each other, because policy only matters when a full cache forces competing blocks to share space. Second, the working set boundary is sharp: the same cyclic workload runs at 4.20 cycles AMAT when its 8 blocks fit L1 and at 16.29 when 12 blocks overflow it. Third, and least intuitive: on that cyclic overflow, Random beats LRU by 24.5% (16.29 to 12.30 AMAT). A working set of 12 blocks cycling through an 8-line L1 is LRU's pathological case, since LRU always evicts exactly the block that the cycle will request next. Random eviction breaks the deterministic alignment and retains a third of the set. FIFO shares LRU's failure because under a pure cyclic scan with no refreshing hits, fill order and recency order coincide.

## Part 2: packet buffer simulator

Packets arrive from a trace (arrival cycle, size) into a fixed-capacity ring buffer; a processor drains it one cache block per tick. Packet sizes are 64 B (one block) or 1,536 B (24 blocks), so large packets generate 24x the cache pressure of small ones. Four buffer sizes place the slot array at each level of the hierarchy:

| Slots | Slot array | Resides in |
|-------|-----------|------------|
| 8     | 12,480 B  | L1 (32 KB) |
| 100   | 156,000 B | L2 (256 KB) |
| 1,000 | 1,560,000 B | L3 (8 MB) |
| 6,000 | 9,360,000 B | RAM |

One nuance the results make visible: the table above is the allocated footprint, but the touched footprint depends on packet size. A 64-byte packet touches only the first cache line of its 1,560-byte slot, so on all-small-packet traces a 100-slot buffer touches 100 lines (6.4 KB) and runs at L1 speed despite a 156 KB allocation. The mixed trace, where 1,536-byte packets walk 24 of each slot's 25 lines, is where allocated and touched footprints converge and the table's residency labels bind.

### Constant-rate trace: the pure cache effect

One 64 B packet every 1,000 cycles. No buffer ever holds more than one packet, so queueing is absent and per-packet latency isolates the memory hierarchy:

| Slots | Served from | Avg latency (cycles/pkt) |
|-------|-------------|--------------------------|
| 8     | L1 100%     | 4.00 |
| 100   | L1 99.99%   | 4.02 |
| 1,000 | L2 99.9%    | 12.19 |
| 6,000 | L2 34% / L3 65.5% | 31.48 |

The latency column is simply the hierarchy's cost ladder read back through the packet pipeline.

### Bursty trace: latency degrades before drops do

Bursts of 8 packets every 1,000 cycles, 64 B each. Every configuration absorbs the bursts with at most cold-start drops (3 of 1M at 8 slots, 0 elsewhere), but latency tells the real story:

| Slots | Avg latency (cycles/pkt) |
|-------|--------------------------|
| 8     | 18.0 |
| 100   | 18.7 |
| 1,000 | 117.2 |
| 6,000 | 2,570.9 |

Drop rate alone reports these four buffers as equivalent. Latency shows a 143x spread. Cache pressure is invisible to the metric most people check first.

### Mixed-size trace: bigger buffers drop more

Packets alternate 64 B / 1,536 B at one per 1,000 cycles. Large packets walk 24 blocks per slot, so the buffer's full footprint is exercised:

| Slots | Resides in | Dropped (of 1M) | Avg latency (cycles/pkt) | AMAT |
|-------|-----------|------------------|--------------------------|------|
| 8     | L1  | 9     | 50.3    | 4.00 |
| 100   | L2  | 98    | 188.0   | 16.05 |
| 1,000 | L3  | 1,342 | 5,303   | 56.38 |
| 6,000 | RAM | 8,223 | 183,641 | 58.43 |

This is the central result. The 6,000-slot buffer has 750x the queueing capacity of the 8-slot buffer and drops 913x more packets, because capacity is not the binding constraint: service rate is. When the slot array spills past L3, every block access costs RAM latency, the processor falls below the arrival rate, and the giant buffer converts its capacity into 183,000 cycles of queueing delay before overflowing anyway. The small buffer's entire working set stays L1-resident and it simply keeps up. Buffer sizing is a working set decision, not a headroom decision.

## Build and test

Requires CMake 3.16+ and a C++20 compiler. No other dependencies; Catch2 is fetched automatically for tests.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The test suite covers the cache model (decomposition, hit/miss, LRU vs FIFO eviction order, eviction vs writeback accounting, seeded determinism, geometry validation), the ring buffer (full-capacity usability, wraparound order, slot identity), and the processor (block walk, hierarchy-sourced timing, latency accounting). Integration tests regenerate the standard traces and assert the exact AMAT figures quoted above, so a change that shifts the published numbers fails CI.

## Reproducing every number

```bash
cd build

# Cache results table
./tracegen --all
./cache_sim trace_sequential.txt trace_strided.txt trace_random.txt \
            trace_workingset.txt trace_workingset_fits.txt trace_mixed.txt

# Packet results tables
./packetgen constant 1000000 1000 packets_constant.txt
./packetgen bursty   1000000 1000 packets_bursty.txt
./packetgen mixed    1000000 1000 packets_mixed.txt
./packet_sim packets_constant.txt
./packet_sim packets_bursty.txt
./packet_sim packets_mixed.txt
```

Both generators are deterministic (`tracegen` under `--seed`, default 42; `packetgen` unconditionally), so traces are never committed: these commands rebuild them byte for byte. Captured output of exactly these runs lives in `results/`.

## Design decisions

**The ring buffer uses an occupancy counter, so all N slots hold packets.** The classic alternative reserves one slot to distinguish full from empty, which exists to spare lock-free SPSC queues a shared counter. This simulator is single threaded, and an "8-slot" buffer that stores 7 packets would put a permanent off-by-one artifact into every drop measurement.

**Packet addresses come from the slot a packet actually occupied**, reported by `dequeue`, not from `sequenceNumber % capacity`. The two diverge after any drop, and the working set claim is about the memory the buffer really touches.

**Latencies live in one place.** `CacheHierarchy::latencyOf(level)` feeds both the AMAT formula and the packet processor's clock, so the two simulators cannot disagree about what an L2 hit costs. The buffer-residency labels are likewise derived from the configured capacities rather than restated.

**Randomness is seeded and cheap.** Random replacement uses one `minstd_rand` per set (a few bytes of state; an `mt19937` per set would cost ~5 KB x 8,192 sets on L3 alone), seeded as `seed + setIndex` so sets do not evict in lockstep. Fixed seeds make every table in this README reproducible, which CI exploits.

**`sizeof(Packet)` is locked by `static_assert`.** Every footprint in the tables assumes the 1,560-byte slot layout; if padding rules ever change it, the build fails instead of the results quietly shifting.

## Limitations

Timing is a cycle-level cost model, not a cycle-accurate microarchitecture: accesses are serialized with fixed per-level costs, so there is no memory-level parallelism, pipelining, or prefetching, and writebacks are counted but not charged cycles. The trace format's access size is parsed but each access is treated as one block probe. Packet slots are `sizeof(Packet)` = 1,560 bytes apart and therefore not cache-line aligned; adjacent slots can share a line, as unpadded descriptor rings do in practice. The hierarchy fills every level on the way down (mostly-inclusive) and models no coherence, since there is a single agent.
