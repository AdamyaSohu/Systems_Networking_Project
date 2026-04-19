// CacheHierarchy.h
#ifndef CACHEHIERARCHY_H
#define CACHEHIERARCHY_H

#include "Cache.h"
#include <string>

class CacheHierarchy {
public:
    // Each level gets its own S, E, B, and replacement policy.
    // Access times in cycles are set here with sensible defaults.
    CacheHierarchy(
        int s1, int e1, int b1, const std::string& p1,
        int s2, int e2, int b2, const std::string& p2,
        int s3, int e3, int b3, const std::string& p3,
        int t1 = 4, int t2 = 12, int t3 = 40, int tMem = 200
    );

    // Route one access through the hierarchy.
    // Returns which level was hit (1, 2, 3, or 4 for RAM).
    int access(uint64_t addr, bool isWrite);

    // Compute AMAT from the three miss rates and access times.
    double amat() const;

    void printStats() const;

    // Public so main can inspect individual levels if needed.
    Cache l1, l2, l3;

private:
    int t1_, t2_, t3_, tMem_;
};

#endif // CACHEHIERARCHY_H