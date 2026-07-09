#include "Cache.h"

#include <bit>
#include <stdexcept>

namespace {
bool isPow2(int v) { return v > 0 && std::has_single_bit(static_cast<unsigned>(v)); }
}  // namespace

std::unique_ptr<ReplacementPolicy> Cache::makePolicy(const std::string& policy,
                                                     uint64_t seed) const {
    if (policy == "lru") return std::make_unique<LRUPolicy>(ways_);
    if (policy == "fifo") return std::make_unique<FIFOPolicy>(ways_);
    if (policy == "random") return std::make_unique<RandomPolicy>(ways_, seed);
    throw std::invalid_argument("unknown replacement policy: " + policy);
}

Cache::Cache(int sets, int ways, int blockBytes, const std::string& policy,
             uint64_t rngSeed)
    : sets_(sets), ways_(ways), blockBytes_(blockBytes) {
    if (!isPow2(sets)) throw std::invalid_argument("set count must be a power of two");
    if (!isPow2(blockBytes)) throw std::invalid_argument("block size must be a power of two");
    if (ways <= 0) throw std::invalid_argument("associativity must be positive");

    offsetBits_ = std::countr_zero(static_cast<unsigned>(blockBytes));
    indexBits_ = std::countr_zero(static_cast<unsigned>(sets));

    table_.resize(sets_);
    policies_.reserve(sets_);
    for (int s = 0; s < sets_; ++s) {
        table_[s].lines.resize(ways_);
        // Offset the seed per set so "random" does not evict the same way
        // index in every set on the same step.
        policies_.push_back(makePolicy(policy, rngSeed + static_cast<uint64_t>(s)));
    }
}

uint64_t Cache::tagOf(uint64_t addr) const { return addr >> (offsetBits_ + indexBits_); }

uint64_t Cache::indexOf(uint64_t addr) const {
    return (addr >> offsetBits_) & ((uint64_t{1} << indexBits_) - 1);
}

bool Cache::access(uint64_t addr, bool isWrite) {
    const uint64_t tag = tagOf(addr);
    CacheSet& set = table_[indexOf(addr)];
    ReplacementPolicy& policy = *policies_[indexOf(addr)];

    for (int w = 0; w < ways_; ++w) {
        CacheLine& line = set.lines[w];
        if (line.valid && line.tag == tag) {
            ++stats.hits;
            if (isWrite) line.dirty = true;
            policy.onHit(w);
            return true;
        }
    }

    ++stats.misses;

    int victim = -1;
    for (int w = 0; w < ways_; ++w) {
        if (!set.lines[w].valid) {
            victim = w;
            break;
        }
    }
    if (victim < 0) {
        victim = policy.victim();
        ++stats.evictions;
        if (set.lines[victim].dirty) ++stats.writebacks;
    }

    set.lines[victim] = {true, isWrite, tag};
    policy.onFill(victim);
    return false;
}

double Cache::hitRate() const {
    const uint64_t total = stats.hits + stats.misses;
    return total == 0 ? 0.0 : static_cast<double>(stats.hits) / static_cast<double>(total);
}

int64_t Cache::capacityBytes() const {
    return static_cast<int64_t>(sets_) * ways_ * blockBytes_;
}
