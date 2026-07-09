#include "PacketProcessor.h"

PacketProcessor::PacketProcessor(CacheHierarchy& cache, int blockBytes)
    : cache_(cache), blockBytes_(blockBytes) {}

int PacketProcessor::tick(RingBuffer& buffer, uint64_t currentCycle) {
    if (!hasPacket_) {
        if (!buffer.dequeue(current_, currentSlot_)) {
            ++stats.idleCycles;
            return 1;
        }
        hasPacket_ = true;
        bytesDone_ = 0;
    }

    // The packet lives at its slot's fixed memory region; walk it one block
    // at a time so cache pressure scales with packet size.
    const uint64_t base = static_cast<uint64_t>(currentSlot_) * sizeof(Packet);
    const int level = cache_.access(base + bytesDone_, /*isWrite=*/false);
    const int cost = cache_.latencyOf(level);
    ++stats.hitsAtLevel[level];
    stats.busyCycles += cost;
    bytesDone_ += blockBytes_;

    if (bytesDone_ >= current_.size) {
        ++stats.packetsProcessed;
        // The packet is finished once this block's cost has elapsed, so its
        // completion time is the end of this tick, not the start.
        stats.totalLatency += (currentCycle + cost) - current_.arrivalCycle;
        hasPacket_ = false;
    }
    return cost;
}
