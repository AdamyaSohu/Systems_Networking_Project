#ifndef REPLACEMENTPOLICY_H
#define REPLACEMENTPOLICY_H

#include <cstdint>
#include <random>
#include <vector>

// Strategy interface for victim selection within a single cache set.
// The Cache owns one policy instance per set, so all state here is per-set.
class ReplacementPolicy {
public:
    virtual ~ReplacementPolicy() = default;

    // Notify the policy that `way` was filled by a miss.
    virtual void onFill(int way) = 0;

    // Notify the policy that `way` was hit.
    virtual void onHit(int way) = 0;

    // Choose the way to evict. Only called when every way in the set is valid.
    virtual int victim() = 0;
};

// LRU and FIFO are both "evict the way with the smallest timestamp"; they
// differ only in which events refresh the timestamp. A per-policy logical
// clock avoids linked-list bookkeeping and keeps state contiguous.
class TimestampPolicy : public ReplacementPolicy {
public:
    explicit TimestampPolicy(int ways) : stamp_(ways, 0) {}

    void onFill(int way) override { stamp_[way] = ++clock_; }

    int victim() override {
        int oldest = 0;
        for (int w = 1; w < static_cast<int>(stamp_.size()); ++w) {
            if (stamp_[w] < stamp_[oldest]) oldest = w;
        }
        return oldest;
    }

protected:
    void touch(int way) { stamp_[way] = ++clock_; }

private:
    std::vector<uint64_t> stamp_;
    uint64_t clock_ = 0;
};

class LRUPolicy final : public TimestampPolicy {
public:
    using TimestampPolicy::TimestampPolicy;
    void onHit(int way) override { touch(way); }  // recency: hits refresh
};

class FIFOPolicy final : public TimestampPolicy {
public:
    using TimestampPolicy::TimestampPolicy;
    void onHit(int) override {}  // residency only: hits do not refresh
};

class RandomPolicy final : public ReplacementPolicy {
public:
    // minstd_rand keeps per-set RNG state to a few bytes; an mt19937 per set
    // would cost ~5 KB x 8192 sets on the L3 alone. Statistical quality is
    // irrelevant here; determinism under a fixed seed is what matters.
    RandomPolicy(int ways, uint64_t seed)
        : rng_(static_cast<std::minstd_rand::result_type>(seed == 0 ? 1 : seed)),
          pick_(0, ways - 1) {}

    void onFill(int) override {}
    void onHit(int) override {}
    int victim() override { return pick_(rng_); }

private:
    std::minstd_rand rng_;
    std::uniform_int_distribution<int> pick_;
};

#endif  // REPLACEMENTPOLICY_H
