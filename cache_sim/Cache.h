// Cache.h
#ifndef CACHE_H
#define CACHE_H

#include "CacheLine.h"
#include "ReplacementPolicy.h"
#include <vector>
#include <memory>
#include <cstdint>
#include <string>

// CacheSet no longer manages LRU itself — the policy does that
struct CacheSet {
    std::vector<CacheLine> lines;
};

class Cache {
public:
    // policyType: "lru", "fifo", or "random"
    Cache(int sets, int ways, int blockBytes, const std::string& policyType);

    bool access(uint64_t addr, bool isWrite);

    double hitRate() const;

    CacheStats stats;

private:
    int S, E;
    int offsetBits, indexBits;

    std::vector<CacheSet> sets_;

    // One policy object per set — each set tracks its own eviction state
    std::vector<std::unique_ptr<ReplacementPolicy>> policies_;

    // Factory function: reads policyType string, returns the right policy object
    std::unique_ptr<ReplacementPolicy> makePolicy(const std::string& policyType);

    uint64_t getTag  (uint64_t addr) const;
    uint64_t getIndex(uint64_t addr) const;
};

#endif // CACHE_H