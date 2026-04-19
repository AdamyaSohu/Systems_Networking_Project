#include "PacketProcessor.h"

PacketProcessor::PacketProcessor(CacheHierarchy& cache, int blockSize, int bufferCapacity)
    : cache_(cache), blockSize_(blockSize),
      bufferCapacity_(bufferCapacity),
      hasPacket_(false), bytesProcessed_(0), packetStartCycle_(0)
{}

int PacketProcessor::processNextBlock(uint64_t baseAddress) {
    uint64_t addr = baseAddress + bytesProcessed_;
    int level = cache_.access(addr, false);

    int cycleCost = 0;
    if      (level == 1) cycleCost = 4;
    else if (level == 2) cycleCost = 12;
    else if (level == 3) cycleCost = 40;
    else                 cycleCost = 200;

    if (level == 1) stats.totalCacheHits++;
    else            stats.totalCacheMisses++;

    bytesProcessed_ += blockSize_;
    return cycleCost;
}

int PacketProcessor::tick(RingBuffer& buffer, uint64_t currentCycle) {
    if (!hasPacket_) {
        if (buffer.isEmpty()) {
            stats.totalIdleCycles++;
            return 1;  // idle costs 1 cycle
        }
        buffer.dequeue(currentPacket_);
        hasPacket_        = true;
        bytesProcessed_   = 0;
        packetStartCycle_ = currentCycle;
    }

    uint64_t baseAddress = (uint64_t)(currentPacket_.sequenceNumber % bufferCapacity_) * sizeof(Packet);
    int cycleCost = processNextBlock(baseAddress);
    stats.totalCyclesProcessed += cycleCost;

    if (bytesProcessed_ >= currentPacket_.size) {
        stats.totalPacketsProcessed++;
        uint64_t latency = currentCycle - currentPacket_.arrivalCycle;
        buffer.stats.totalProcessed++;
        buffer.stats.totalLatency += latency;
        hasPacket_      = false;
        bytesProcessed_ = 0;
    }

    return cycleCost;
}