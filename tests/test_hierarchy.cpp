#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

#include "cache_sim/CacheHierarchy.h"

namespace {
CacheHierarchy makeSmall() {
    return CacheHierarchy({4, 2, 16, "lru"}, {8, 4, 16, "lru"}, {16, 8, 16, "lru"});
}
}  // namespace

TEST_CASE("a cold access misses every level and is served by RAM") {
    CacheHierarchy h = makeSmall();
    CHECK(h.access(0x1000, false) == CacheHierarchy::kRam);
    CHECK(h.l1.stats.misses == 1);
    CHECK(h.l2.stats.misses == 1);
    CHECK(h.l3.stats.misses == 1);
}

TEST_CASE("every level fills on the way down, so a re-access hits L1") {
    CacheHierarchy h = makeSmall();
    h.access(0x1000, false);
    CHECK(h.access(0x1000, false) == CacheHierarchy::kL1);
    CHECK(h.l1.stats.hits == 1);
    // L2 and L3 see nothing on an L1 hit.
    CHECK(h.l2.stats.hits + h.l2.stats.misses == 1);
}

TEST_CASE("latencyOf reports the configured cost per level and rejects others") {
    CacheHierarchy h = makeSmall();
    CHECK(h.latencyOf(CacheHierarchy::kL1) == 4);
    CHECK(h.latencyOf(CacheHierarchy::kL2) == 12);
    CHECK(h.latencyOf(CacheHierarchy::kL3) == 40);
    CHECK(h.latencyOf(CacheHierarchy::kRam) == 200);
    CHECK_THROWS_AS(h.latencyOf(5), std::invalid_argument);
}

TEST_CASE("AMAT combines local miss rates with per-level latency") {
    CacheHierarchy h = makeSmall();
    h.access(0x1000, false);  // cold: misses L1, L2, L3
    h.access(0x1000, false);  // hits L1
    // L1 hit rate 1/2; L2 and L3 saw one access each, both misses.
    // AMAT = 4 + 0.5 * (12 + 1.0 * (40 + 1.0 * 200)) = 130.
    CHECK(h.amat() == Catch::Approx(130.0));
}

TEST_CASE("custom latencies flow through both latencyOf and AMAT") {
    CacheHierarchy h({4, 2, 16, "lru"}, {8, 4, 16, "lru"}, {16, 8, 16, "lru"},
                     LatencyConfig{1, 2, 3, 10});
    CHECK(h.latencyOf(CacheHierarchy::kRam) == 10);
    h.access(0x0, false);  // all miss
    CHECK(h.amat() == Catch::Approx(1 + 2 + 3 + 10));
}
