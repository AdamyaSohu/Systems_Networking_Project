# multi-level-cache-simulator
A configurable C++ cache simulator modeling multi-level memory hierarchies and replacement policies, with experiments on workload behavior and performance.
Multi-Level Cache Simulator

A configurable cache simulator written in C++ that models multi-level memory hierarchies and explores how workload patterns affect cache performance.

Overview

This project simulates a three-level cache hierarchy (L1, L2, L3) with configurable parameters:

Number of sets (S)
Associativity (E)
Block size (B)
Replacement policy (LRU, FIFO, Random)

Each memory access is processed through the hierarchy, and the simulator tracks hit/miss behavior and computes Average Memory Access Time (AMAT).


System Design
Set-associative cache with configurable parameters
Multi-level hierarchy (L1 → L2 → L3 → main memory)
Address decomposition into tag, index, and offset using bit-level operations
Replacement policies implemented using a shared abstraction
Replacement Policies
LRU (Least Recently Used)
FIFO (First In First Out)
Random

All policies share the same hit/miss logic and differ only in eviction behavior.

Workloads

The simulator evaluates different access patterns:

Sequential — no reuse, worst-case for caching
Strided — breaks spatial locality
Random — no predictable locality
Working set — repeated access within a fixed set of blocks
Mixed — combination of structured and random access
Key Result

One result stood out.

When the working set slightly exceeded cache capacity and accesses were cyclic:

LRU achieved 0% L1 hit rate
Random achieved ~33% L1 hit rate

LRU kept evicting data right before it was needed again. Random eviction avoided this pattern and performed better.

This shows that replacement policies do not behave the same across workloads. In some cases, simple policies can outperform more structured ones.

What this shows
Cache performance is driven by workload patterns
Replacement policy effectiveness depends on access behavior
Working set boundaries create sharp changes in performance
Assumptions like “LRU is better than random” do not always hold
How to run
Clone the repository
Build the project (using your preferred build system)
Run the simulator with desired cache parameters and workload
Full write-up

A detailed analysis of the design, experiments, and results is available in the project write-up
