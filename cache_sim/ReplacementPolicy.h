// ReplacementPolicy.h
#ifndef REPLACEMENTPOLICY_H
#define REPLACEMENTPOLICY_H

#include <list>
#include <queue>
#include <cstdlib>

// --- Base class ---
// Defines the interface every replacement policy must implement.
// Cannot be instantiated directly.
class ReplacementPolicy {
public:
    virtual void onFill(int wayIndex) = 0;
    // Called when a cache line is filled into wayIndex.
    // The policy should record that this way was just used.

    virtual void onHit(int wayIndex) = 0;
    // Called when wayIndex was a hit.
    // LRU needs this. FIFO and random don't, but must still implement it.

    virtual int getVictim() = 0;
    // Called when we need to evict someone.
    // Returns the index of the way to evict.

    virtual ~ReplacementPolicy() = default;
};


// --- LRU ---
class LRUPolicy : public ReplacementPolicy {
public:
    LRUPolicy(int ways) {
        for (int i = 0; i < ways; ++i)
            order_.push_back(i);
    }

    void onFill(int wayIndex) override {
        order_.remove(wayIndex);
        order_.push_front(wayIndex);
    }

    void onHit(int wayIndex) override {
        order_.remove(wayIndex);
        order_.push_front(wayIndex);
    }

    int getVictim() override {
        return order_.back();
    }

private:
    std::list<int> order_;
};


// --- FIFO ---
class FIFOPolicy : public ReplacementPolicy {
public:
    FIFOPolicy(int ways) {
        for (int i = 0; i < ways; ++i)
            order_.push(i);
    }

    void onFill(int wayIndex) override {
        order_.push(wayIndex);
    }

    void onHit(int wayIndex) override {
        // FIFO doesn't care about hits
    }

    int getVictim() override {
        int victim = order_.front();
        order_.pop();
        return victim;
    }

private:
    std::queue<int> order_;
};


// --- Random ---
class RandomPolicy : public ReplacementPolicy {
public:
    RandomPolicy(int ways) : ways_(ways) {}

    void onFill(int wayIndex) override {
        // nothing to track
    }

    void onHit(int wayIndex) override {
        // nothing to track
    }

    int getVictim() override {
        return rand() % ways_;
    }

private:
    int ways_;
};

#endif // REPLACEMENTPOLICY_H