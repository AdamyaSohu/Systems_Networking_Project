#ifndef RINGBUFFER_H
#define RINGBUFFER_H
#include "Packet.h"
#include <cstdint>
#include <vector>

struct RingBufferStats {
    uint64_t totalArrived   = 0;
    uint64_t totalProcessed = 0;
    uint64_t totalDropped   = 0;
    uint64_t totalCycles    = 0;
    uint64_t totalLatency   = 0;

    double throughput() const {
        if (totalCycles == 0) return 0.0;
        return (double)totalProcessed / totalCycles;
    }
    double dropRate() const {
        if (totalArrived == 0) return 0.0;
        return (double)totalDropped / totalArrived;
    }
    double avgLatency() const {
        if (totalProcessed == 0) return 0.0;
        return (double)totalLatency / totalProcessed;
    }
};

class RingBuffer {
public:
    RingBuffer(int capacity);
    bool enqueue(const Packet& pkt);
    bool dequeue(Packet& out);
    bool isEmpty() const;
    bool isFull()  const;
    int  occupancy() const;
    size_t footprintBytes() const;

    RingBufferStats stats;

private:
    std::vector<Packet> buffer_;
    int capacity_;
    int readPos_;
    int writePos_;
    int advance(int index) const { return (index + 1) % capacity_; }
};

#endif // RINGBUFFER_H