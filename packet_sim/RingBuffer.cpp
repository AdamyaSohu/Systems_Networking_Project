#include "RingBuffer.h"

RingBuffer::RingBuffer(int capacity)
    : capacity_(capacity), readPos_(0), writePos_(0)
{
    buffer_.resize(capacity_);
}

bool RingBuffer::isEmpty() const { return readPos_ == writePos_; }
bool RingBuffer::isFull()  const { return advance(writePos_) == readPos_; }

int RingBuffer::occupancy() const {
    return (writePos_ - readPos_ + capacity_) % capacity_;
}

size_t RingBuffer::footprintBytes() const {
    return capacity_ * sizeof(Packet);
}

bool RingBuffer::enqueue(const Packet& pkt) {
    stats.totalArrived++;
    if (isFull()) { stats.totalDropped++; return false; }
    buffer_[writePos_] = pkt;
    writePos_ = advance(writePos_);
    return true;
}

bool RingBuffer::dequeue(Packet& out) {
    if (isEmpty()) return false;
    out = buffer_[readPos_];
    readPos_ = advance(readPos_);
    return true;
}