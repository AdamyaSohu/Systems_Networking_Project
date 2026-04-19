// Cache.cpp
#include "Cache.h"
#include <stdexcept>   // std::invalid_argument

// --- Factory function ---
std::unique_ptr<ReplacementPolicy> Cache::makePolicy(const std::string& type) {
    if      (type == "lru")    return std::make_unique<LRUPolicy>(E);
    else if (type == "fifo")   return std::make_unique<FIFOPolicy>(E);
    else if (type == "random") return std::make_unique<RandomPolicy>(E);
    else throw std::invalid_argument("Unknown policy: " + type);
    // If someone passes a typo like "lur", we crash immediately with a
    // clear error message rather than silently doing something wrong.
}

// --- Constructor ---
Cache::Cache(int sets, int ways, int blockBytes, const std::string& policyType)
    : S(sets), E(ways),
      offsetBits(__builtin_ctz(blockBytes)),
      indexBits (__builtin_ctz(sets))
{
    sets_.resize(S);
    policies_.resize(S);

    for (int i = 0; i < S; ++i) {
        sets_[i].lines.resize(E);
        policies_[i] = makePolicy(policyType);
        // Each set gets its own fresh policy object.
        // They are independent — set 0's LRU order never affects set 1's.
    }
}

// --- Address helpers ---
uint64_t Cache::getTag(uint64_t addr) const {
    return addr >> (offsetBits + indexBits);
}

uint64_t Cache::getIndex(uint64_t addr) const {
    return (addr >> offsetBits) & ((1 << indexBits) - 1);
}

// --- access() ---
bool Cache::access(uint64_t addr, bool isWrite) {
    uint64_t tag   = getTag(addr);
    uint64_t index = getIndex(addr);

    CacheSet& cset             = sets_[index];
    ReplacementPolicy& policy  = *policies_[index];
    // The & here dereferences the unique_ptr to get a reference.
    // We use a reference so we don't have to write *policies_[index]
    // every single time we call a method on it.

    // --- Hit check ---
    for (int i = 0; i < E; ++i) {
        if (cset.lines[i].valid && cset.lines[i].tag == tag) {
            stats.hits++;
            if (isWrite) cset.lines[i].dirty = true;
            policy.onHit(i);   // tell the policy this way was just used
            return true;
        }
    }

    // --- Miss ---
    stats.misses++;

    // Find a free slot first
    int victim = -1;
    for (int i = 0; i < E; ++i) {
        if (!cset.lines[i].valid) { victim = i; break; }
    }

    // No free slot — ask the policy who to evict
    if (victim == -1) {
        victim = policy.getVictim();
        if (cset.lines[victim].dirty) stats.evictions++;
    }

    // Fill the victim
    cset.lines[victim] = { true, isWrite, tag };
    policy.onFill(victim);   // tell the policy this way was just filled
    return false;
}

// --- hitRate() ---
double Cache::hitRate() const {
    uint64_t total = stats.hits + stats.misses;
    return total == 0 ? 0.0 : (double)stats.hits / total;
}