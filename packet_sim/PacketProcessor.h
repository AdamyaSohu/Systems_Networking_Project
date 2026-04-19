#ifndef PACKETPROCESSOR_H
#define PACKETPROCESSOR_H
#include "Packet.h"
#include "RingBuffer.h"
#include "../cache_sim/CacheHierarchy.h"
#include <cstdint>

struct ProcessorStats {
    uint64_t totalPacketsProcessed = 0;
    uint64_t totalCyclesProcessed  = 0;
    uint64_t totalIdleCycles       = 0;
    uint64_t totalCacheHits        = 0;
    uint64_t totalCacheMisses      = 0;

    double cacheHitRate() const {
        uint64_t total = totalCacheHits + totalCacheMisses;
        return total == 0 ? 0.0 : (double)totalCacheHits / total;
    }
    double avgCyclesPerPacket() const {
        if (totalPacketsProcessed == 0) return 0.0;
        return (double)totalCyclesProcessed / totalPacketsProcessed;
    }
};

class PacketProcessor {
public:
    PacketProcessor(CacheHierarchy& cache, int blockSize, int bufferCapacity);
    int tick(RingBuffer& buffer, uint64_t currentCycle);
    ProcessorStats stats;

private:
    CacheHierarchy& cache_;
    int     blockSize_;
    int bufferCapacity_;
    Packet  currentPacket_;
    bool    hasPacket_        = false;
    int     bytesProcessed_   = 0;
    uint64_t packetStartCycle_ = 0;
    

    int processNextBlock(uint64_t baseAddress);
};

#endif // PACKETPROCESSOR_H