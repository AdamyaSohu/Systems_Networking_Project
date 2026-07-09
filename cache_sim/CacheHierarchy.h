#ifndef CACHEHIERARCHY_H
#define CACHEHIERARCHY_H

#include <cstdint>
#include <string>

#include "Cache.h"

// Geometry and policy for one cache level.
struct LevelConfig {
    int sets;
    int ways;
    int blockBytes;
    std::string policy;
};

// Access latency, in cycles, for each stage of the hierarchy.
struct LatencyConfig {
    int l1 = 4;
    int l2 = 12;
    int l3 = 40;
    int ram = 200;
};

// Three-level inclusive-fill hierarchy. A miss at level N is presented to
// level N+1, and every level a request passes through fills the block, so a
// re-access hits at L1.
class CacheHierarchy {
public:
    static constexpr int kL1 = 1;
    static constexpr int kL2 = 2;
    static constexpr int kL3 = 3;
    static constexpr int kRam = 4;

    CacheHierarchy(const LevelConfig& l1Cfg, const LevelConfig& l2Cfg,
                   const LevelConfig& l3Cfg, const LatencyConfig& lat = {},
                   uint64_t rngSeed = Cache::kDefaultSeed);

    // Route one access through the hierarchy. Returns the level that
    // satisfied it: kL1, kL2, kL3, or kRam.
    int access(uint64_t addr, bool isWrite);

    // Cycle cost of a request satisfied at `level`. This is the only place
    // latencies live; the packet simulator's timing comes from here too.
    int latencyOf(int level) const;

    // AMAT = T1 + MR1 * (T2 + MR2 * (T3 + MR3 * Tram)), where each MR is the
    // local miss rate of that level (misses / accesses reaching the level).
    double amat() const;

    void printStats() const;

    Cache l1, l2, l3;

private:
    LatencyConfig lat_;
};

#endif  // CACHEHIERARCHY_H
