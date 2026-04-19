// CacheHierarchy.cpp
#include "CacheHierarchy.h"
#include <iostream>

// --- Constructor ---
CacheHierarchy::CacheHierarchy(
    int s1, int e1, int b1, const std::string& p1,
    int s2, int e2, int b2, const std::string& p2,
    int s3, int e3, int b3, const std::string& p3,
    int t1, int t2, int t3, int tMem
)
    : l1(s1, e1, b1, p1),
      l2(s2, e2, b2, p2),
      l3(s3, e3, b3, p3),
      t1_(t1), t2_(t2), t3_(t3), tMem_(tMem)
{}
// The colon syntax here is the initializer list again — same as Cache's
// constructor. We're constructing l1, l2, l3 directly with their parameters
// before the constructor body runs. This is the correct way to initialize
// member objects that themselves have constructors.

// --- access() ---
int CacheHierarchy::access(uint64_t addr, bool isWrite) {

    // Try L1 first.
    if (l1.access(addr, isWrite)) return 1;   // hit in L1 — stop here

    // L1 missed. Try L2.
    if (l2.access(addr, isWrite)) return 2;   // hit in L2 — stop here

    // L2 missed. Try L3.
    if (l3.access(addr, isWrite)) return 3;   // hit in L3 — stop here

    // L3 missed. Goes to RAM — nothing to simulate, just count it.
    return 4;
}

// --- amat() ---
double CacheHierarchy::amat() const {
    double mr1 = 1.0 - l1.hitRate();
    double mr2 = 1.0 - l2.hitRate();
    double mr3 = 1.0 - l3.hitRate();

    return t1_ + mr1 * (t2_ + mr2 * (t3_ + mr3 * tMem_));
}

// --- printStats() ---
void CacheHierarchy::printStats() const {
    auto printLevel = [](const std::string& name, const Cache& c) {
        uint64_t total = c.stats.hits + c.stats.misses;
        std::cout << name << "\n";
        std::cout << "  Accesses:  " << total            << "\n";
        std::cout << "  Hits:      " << c.stats.hits     << "\n";
        std::cout << "  Misses:    " << c.stats.misses   << "\n";
        std::cout << "  Evictions: " << c.stats.evictions<< "\n";
        std::cout << "  Hit rate:  " << c.hitRate() * 100<< "%\n";
    };

    printLevel("L1", l1);
    printLevel("L2", l2);
    printLevel("L3", l3);
    std::cout << "AMAT: " << amat() << " cycles\n";
}