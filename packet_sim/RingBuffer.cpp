#include "RingBuffer.h"

#include <stdexcept>

RingBuffer::RingBuffer(int capacity) : capacity_(capacity) {
    if (capacity <= 0) throw std::invalid_argument("ring buffer capacity must be positive");
    slots_.resize(capacity_);
}

int RingBuffer::enqueue(const Packet& pkt) {
    ++stats.totalArrived;
    if (isFull()) {
        ++stats.totalDropped;
        return -1;
    }
    const int slot = writePos_;
    slots_[slot] = pkt;
    writePos_ = advance(writePos_);
    ++count_;
    return slot;
}

bool RingBuffer::dequeue(Packet& out, int& slot) {
    if (isEmpty()) return false;
    slot = readPos_;
    out = slots_[slot];
    readPos_ = advance(readPos_);
    --count_;
    return true;
}
