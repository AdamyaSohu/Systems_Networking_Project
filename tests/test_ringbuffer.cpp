#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

#include "packet_sim/RingBuffer.h"

namespace {
Packet pkt(uint32_t seq) { return Packet(seq, /*arrival=*/0, /*bytes=*/64); }
}  // namespace

TEST_CASE("all capacity slots are usable: N enqueues succeed, N+1 drops") {
    RingBuffer rb(4);
    for (uint32_t i = 0; i < 4; ++i) CHECK(rb.enqueue(pkt(i)) >= 0);
    CHECK(rb.isFull());
    CHECK(rb.enqueue(pkt(99)) == -1);
    CHECK(rb.stats.totalArrived == 5);
    CHECK(rb.stats.totalDropped == 1);
}

TEST_CASE("dequeue preserves FIFO order across wraparound") {
    RingBuffer rb(3);
    rb.enqueue(pkt(0));
    rb.enqueue(pkt(1));

    Packet out;
    int slot = -1;
    REQUIRE(rb.dequeue(out, slot));
    CHECK(out.sequenceNumber == 0);

    rb.enqueue(pkt(2));
    rb.enqueue(pkt(3));  // wraps into the freed slot

    uint32_t expected = 1;
    while (rb.dequeue(out, slot)) CHECK(out.sequenceNumber == expected++);
    CHECK(expected == 4);
}

TEST_CASE("enqueue and dequeue agree on the slot a packet occupied") {
    RingBuffer rb(2);
    const int s0 = rb.enqueue(pkt(0));
    const int s1 = rb.enqueue(pkt(1));
    CHECK(s0 == 0);
    CHECK(s1 == 1);

    Packet out;
    int slot = -1;
    rb.dequeue(out, slot);
    CHECK(slot == s0);
    // Freed slot 0 is reused by the next enqueue.
    CHECK(rb.enqueue(pkt(2)) == 0);
    rb.dequeue(out, slot);
    CHECK(slot == s1);
}

TEST_CASE("occupancy tracks enqueues and dequeues") {
    RingBuffer rb(3);
    CHECK(rb.isEmpty());
    rb.enqueue(pkt(0));
    rb.enqueue(pkt(1));
    CHECK(rb.occupancy() == 2);
    Packet out;
    int slot;
    rb.dequeue(out, slot);
    CHECK(rb.occupancy() == 1);
}

TEST_CASE("dropped packets are not stored and do not disturb order") {
    RingBuffer rb(2);
    rb.enqueue(pkt(0));
    rb.enqueue(pkt(1));
    rb.enqueue(pkt(2));  // dropped
    Packet out;
    int slot;
    rb.dequeue(out, slot);
    rb.dequeue(out, slot);
    CHECK(out.sequenceNumber == 1);
    CHECK(rb.isEmpty());
}

TEST_CASE("constructor rejects non-positive capacity") {
    CHECK_THROWS_AS(RingBuffer(0), std::invalid_argument);
    CHECK_THROWS_AS(RingBuffer(-3), std::invalid_argument);
}

TEST_CASE("footprint is capacity times slot size") {
    RingBuffer rb(8);
    CHECK(rb.footprintBytes() == 8 * static_cast<int64_t>(sizeof(Packet)));
    CHECK(sizeof(Packet) == 1560);  // published tables depend on this layout
}
