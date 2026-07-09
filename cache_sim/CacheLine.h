#ifndef CACHELINE_H
#define CACHELINE_H

#include <cstdint>

struct CacheLine {
    bool valid = false;
    bool dirty = false;
    uint64_t tag = 0;
};

struct CacheStats {
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t evictions = 0;   // any valid line replaced
    uint64_t writebacks = 0;  // evictions of dirty lines only
};

#endif  // CACHELINE_H
