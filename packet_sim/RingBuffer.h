#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <cstdint>
#include <vector>

#include "Packet.h"

struct RingBufferStats {
    uint64_t totalArrived = 0;
    uint64_t totalDropped = 0;

    double dropRate() const {
        return totalArrived == 0 ? 0.0
                                 : static_cast<double>(totalDropped) / totalArrived;
    }
};

// Fixed-capacity FIFO of packets backed by contiguous slots.
//
// Occupancy is tracked with an explicit count, so all `capacity` slots are
// usable. The classic reserve-one-slot idiom (full when write+1 == read)
// exists to let lock-free SPSC queues avoid a shared counter; this simulator
// is single threaded, and an "8 slot" buffer that holds 7 packets would put
// an off-by-one artifact in every drop measurement.
class RingBuffer {
public:
    explicit RingBuffer(int capacity);

    // Returns the slot index the packet was stored in, or -1 if the buffer
    // was full and the packet was dropped. The slot index is what the
    // processor uses as the packet's memory location, so it must reflect
    // where the packet actually landed, not its sequence number.
    int enqueue(const Packet& pkt);

    // On success fills `out` and `slot` and returns true.
    bool dequeue(Packet& out, int& slot);

    bool isEmpty() const { return count_ == 0; }
    bool isFull() const { return count_ == capacity_; }
    int occupancy() const { return count_; }
    int capacity() const { return capacity_; }
    int64_t footprintBytes() const {
        return static_cast<int64_t>(capacity_) * sizeof(Packet);
    }

    RingBufferStats stats;

private:
    std::vector<Packet> slots_;
    int capacity_;
    int readPos_ = 0;
    int writePos_ = 0;
    int count_ = 0;

    int advance(int i) const { return (i + 1) % capacity_; }
};

#endif  // RINGBUFFER_H
