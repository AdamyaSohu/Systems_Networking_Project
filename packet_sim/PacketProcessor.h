#ifndef PACKETPROCESSOR_H
#define PACKETPROCESSOR_H

#include <cstdint>

#include "../cache_sim/CacheHierarchy.h"
#include "Packet.h"
#include "RingBuffer.h"

struct ProcessorStats {
    uint64_t packetsProcessed = 0;
    uint64_t busyCycles = 0;
    uint64_t idleCycles = 0;
    uint64_t totalLatency = 0;             // completion cycle - trace arrival cycle
    uint64_t hitsAtLevel[5] = {0, 0, 0, 0, 0};  // indexed by CacheHierarchy level

    double l1HitRate() const {
        uint64_t total = 0;
        for (int lvl = CacheHierarchy::kL1; lvl <= CacheHierarchy::kRam; ++lvl)
            total += hitsAtLevel[lvl];
        return total == 0
                   ? 0.0
                   : static_cast<double>(hitsAtLevel[CacheHierarchy::kL1]) / total;
    }
    double avgCyclesPerPacket() const {
        return packetsProcessed == 0
                   ? 0.0
                   : static_cast<double>(busyCycles) / packetsProcessed;
    }
    double avgLatency() const {
        return packetsProcessed == 0
                   ? 0.0
                   : static_cast<double>(totalLatency) / packetsProcessed;
    }
};

// Drains a RingBuffer one cache block per tick. Timing comes from the cache
// hierarchy: each 64-byte block of a packet is one access at the address of
// the slot the packet occupied, and the tick costs whatever the hierarchy
// says that access costs. This is the coupling under study: the buffer's
// slot array is the processor's working set.
class PacketProcessor {
public:
    PacketProcessor(CacheHierarchy& cache, int blockBytes);

    // Advance one step. Returns the number of cycles the step consumed.
    int tick(RingBuffer& buffer, uint64_t currentCycle);

    bool busy() const { return hasPacket_; }

    ProcessorStats stats;

private:
    CacheHierarchy& cache_;
    int blockBytes_;
    Packet current_;
    int currentSlot_ = -1;
    bool hasPacket_ = false;
    int bytesDone_ = 0;
};

#endif  // PACKETPROCESSOR_H
