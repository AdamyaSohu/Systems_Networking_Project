#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "packet_sim/PacketProcessor.h"

namespace {
CacheHierarchy makeHierarchy() {
    return CacheHierarchy({64, 8, 64, "lru"}, {512, 8, 64, "lru"}, {8192, 16, 64, "lru"});
}
}  // namespace

TEST_CASE("an idle tick costs one cycle and touches no cache state") {
    CacheHierarchy h = makeHierarchy();
    RingBuffer rb(4);
    PacketProcessor p(h, 64);
    CHECK(p.tick(rb, 0) == 1);
    CHECK(p.stats.idleCycles == 1);
    CHECK(p.stats.packetsProcessed == 0);
    CHECK(h.l1.stats.hits + h.l1.stats.misses == 0);
}

TEST_CASE("a 64-byte packet is one block: one access, one tick to finish") {
    CacheHierarchy h = makeHierarchy();
    RingBuffer rb(4);
    PacketProcessor p(h, 64);
    rb.enqueue(Packet(0, /*arrival=*/0, /*bytes=*/64));

    const int cost = p.tick(rb, 0);
    CHECK(cost == h.latencyOf(CacheHierarchy::kRam));  // cold miss
    CHECK(p.stats.packetsProcessed == 1);
    CHECK(p.stats.hitsAtLevel[CacheHierarchy::kRam] == 1);
    CHECK_FALSE(p.busy());
}

TEST_CASE("a 1536-byte packet takes 24 block accesses to complete") {
    CacheHierarchy h = makeHierarchy();
    RingBuffer rb(4);
    PacketProcessor p(h, 64);
    rb.enqueue(Packet(0, 0, 1536));

    int ticks = 0;
    while (!rb.isEmpty() || p.busy()) {
        p.tick(rb, 0);
        ++ticks;
    }
    CHECK(ticks == 24);
    CHECK(p.stats.packetsProcessed == 1);
}

TEST_CASE("timing comes from the hierarchy: a rewalked slot runs at L1 speed") {
    CacheHierarchy h = makeHierarchy();
    RingBuffer rb(4);
    PacketProcessor p(h, 64);

    // One full lap of the ring: packets 0-3 fault their slots in (RAM cost),
    // then packet 4 wraps back to slot 0, whose line is now L1 resident.
    for (uint32_t seq = 0; seq < 4; ++seq) {
        rb.enqueue(Packet(seq, 0, 64));
        CHECK(p.tick(rb, 0) == h.latencyOf(CacheHierarchy::kRam));
    }
    rb.enqueue(Packet(4, 0, 64));
    const int warmCost = p.tick(rb, 100);

    CHECK(warmCost == h.latencyOf(CacheHierarchy::kL1));
    CHECK(p.stats.hitsAtLevel[CacheHierarchy::kL1] == 1);
    CHECK(p.stats.l1HitRate() == Catch::Approx(1.0 / 5.0));
}

TEST_CASE("latency spans trace arrival to completion, including service time") {
    CacheHierarchy h = makeHierarchy();
    RingBuffer rb(4);
    PacketProcessor p(h, 64);

    rb.enqueue(Packet(0, /*arrival=*/40, /*bytes=*/64));
    const uint64_t tickStart = 100;  // packet waited 60 cycles in the queue
    const int cost = p.tick(rb, tickStart);

    CHECK(p.stats.packetsProcessed == 1);
    CHECK(p.stats.totalLatency == (tickStart + cost) - 40);
}

TEST_CASE("busy cycles accumulate access costs, not idle time") {
    CacheHierarchy h = makeHierarchy();
    RingBuffer rb(4);
    PacketProcessor p(h, 64);
    p.tick(rb, 0);  // idle
    rb.enqueue(Packet(0, 0, 64));
    const int cost = p.tick(rb, 1);
    CHECK(p.stats.busyCycles == static_cast<uint64_t>(cost));
    CHECK(p.stats.idleCycles == 1);
}
