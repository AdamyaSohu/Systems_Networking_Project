#ifndef CACHE_H
#define CACHE_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "CacheLine.h"
#include "ReplacementPolicy.h"

// A single set-associative cache level, parameterized by set count, ways,
// block size, and replacement policy ("lru", "fifo", or "random").
//
// Eviction state lives in one ReplacementPolicy instance per set, so a hit in
// set 0 never perturbs set 1's ordering. This is also what makes policies
// swappable without touching the hit/miss path.
class Cache {
public:
    // Throws std::invalid_argument unless sets and blockBytes are powers of
    // two and all three geometry values are positive. Failing loudly here is
    // deliberate: a non power-of-two geometry would silently corrupt the
    // tag/index/offset decomposition below.
    Cache(int sets, int ways, int blockBytes, const std::string& policy,
          uint64_t rngSeed = kDefaultSeed);

    // Simulate one access. Returns true on hit. On a miss the block is
    // filled into this cache (evicting if the set is full).
    bool access(uint64_t addr, bool isWrite);

    double hitRate() const;
    int64_t capacityBytes() const;

    // Field decomposition, exposed for tests.
    uint64_t tagOf(uint64_t addr) const;
    uint64_t indexOf(uint64_t addr) const;

    CacheStats stats;

    static constexpr uint64_t kDefaultSeed = 42;

private:
    struct CacheSet {
        std::vector<CacheLine> lines;
    };

    std::unique_ptr<ReplacementPolicy> makePolicy(const std::string& policy,
                                                  uint64_t seed) const;

    int sets_;
    int ways_;
    int blockBytes_;
    int offsetBits_;
    int indexBits_;

    std::vector<CacheSet> table_;
    std::vector<std::unique_ptr<ReplacementPolicy>> policies_;
};

#endif  // CACHE_H
