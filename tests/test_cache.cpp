#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

#include "cache_sim/Cache.h"

// Geometry used throughout: 4 sets x 2 ways x 16 B blocks.
// offsetBits = 4, indexBits = 2, so bits [3:0] offset, [5:4] index, rest tag.
namespace {
Cache makeCache(const std::string& policy = "lru") { return Cache(4, 2, 16, policy); }

// Address with a given tag that lands in a given set.
uint64_t addrFor(uint64_t tag, uint64_t set) { return (tag << 6) | (set << 4); }
}  // namespace

TEST_CASE("address decomposition splits offset, index, and tag fields") {
    Cache c = makeCache();
    const uint64_t addr = addrFor(/*tag=*/0xAB, /*set=*/2) | 0x7;  // offset bits set
    CHECK(c.indexOf(addr) == 2);
    CHECK(c.tagOf(addr) == 0xAB);
}

TEST_CASE("second access to the same block hits") {
    Cache c = makeCache();
    CHECK_FALSE(c.access(0x100, false));  // cold miss
    CHECK(c.access(0x100, false));        // now resident
    CHECK(c.access(0x10F, false));        // same block, different offset
    CHECK(c.stats.hits == 2);
    CHECK(c.stats.misses == 1);
}

TEST_CASE("misses fill invalid ways before evicting anything") {
    Cache c = makeCache();
    c.access(addrFor(1, 0), false);
    c.access(addrFor(2, 0), false);  // both ways of set 0 now valid
    CHECK(c.stats.evictions == 0);
}

TEST_CASE("LRU evicts the least recently touched way") {
    Cache c = makeCache("lru");
    c.access(addrFor(1, 0), false);  // fill A
    c.access(addrFor(2, 0), false);  // fill B
    c.access(addrFor(1, 0), false);  // touch A, so B is now LRU
    c.access(addrFor(3, 0), false);  // fill C, evicting B

    CHECK(c.access(addrFor(1, 0), false));        // A survived
    CHECK(c.access(addrFor(3, 0), false));        // C resident
    CHECK_FALSE(c.access(addrFor(2, 0), false));  // B was evicted
}

TEST_CASE("FIFO evicts by fill order, ignoring hits") {
    Cache c = makeCache("fifo");
    c.access(addrFor(1, 0), false);  // fill A
    c.access(addrFor(2, 0), false);  // fill B
    c.access(addrFor(1, 0), false);  // hit A; FIFO must not refresh it
    c.access(addrFor(3, 0), false);  // fill C, evicting A (oldest fill)

    CHECK_FALSE(c.access(addrFor(1, 0), false));  // A was evicted despite the hit
}

TEST_CASE("evictions count every replacement; writebacks only dirty lines") {
    Cache c = makeCache("lru");
    c.access(addrFor(1, 0), /*isWrite=*/true);   // A, dirty
    c.access(addrFor(2, 0), /*isWrite=*/false);  // B, clean
    c.access(addrFor(3, 0), false);              // evicts A (dirty)
    c.access(addrFor(4, 0), false);              // evicts B (clean)

    CHECK(c.stats.evictions == 2);
    CHECK(c.stats.writebacks == 1);
}

TEST_CASE("a hit on a write marks the line dirty for its eventual eviction") {
    Cache c = makeCache("lru");
    c.access(addrFor(1, 0), false);  // A arrives clean
    c.access(addrFor(1, 0), true);   // write hit dirties it
    c.access(addrFor(2, 0), false);
    c.access(addrFor(3, 0), false);  // evicts A
    CHECK(c.stats.writebacks == 1);
}

TEST_CASE("random policy is deterministic under a fixed seed") {
    const auto run = [](uint64_t seed) {
        Cache c(4, 2, 16, "random", seed);
        for (int i = 0; i < 500; ++i) c.access(addrFor(i % 5, i % 4), false);
        return c.stats.hits;
    };
    CHECK(run(42) == run(42));
}

TEST_CASE("constructor rejects invalid geometry and unknown policies") {
    CHECK_THROWS_AS(Cache(3, 2, 16, "lru"), std::invalid_argument);   // sets not pow2
    CHECK_THROWS_AS(Cache(4, 0, 16, "lru"), std::invalid_argument);   // zero ways
    CHECK_THROWS_AS(Cache(4, 2, 17, "lru"), std::invalid_argument);   // block not pow2
    CHECK_THROWS_AS(Cache(4, 2, 16, "lur"), std::invalid_argument);   // typo policy
}

TEST_CASE("hit rate is hits over total accesses") {
    Cache c = makeCache();
    c.access(0x0, false);  // miss
    c.access(0x0, false);  // hit
    c.access(0x0, false);  // hit
    CHECK(c.hitRate() == Catch::Approx(2.0 / 3.0).epsilon(1e-12));
}
