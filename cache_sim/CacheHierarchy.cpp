#include "CacheHierarchy.h"

#include <iostream>
#include <stdexcept>

CacheHierarchy::CacheHierarchy(const LevelConfig& l1Cfg, const LevelConfig& l2Cfg,
                               const LevelConfig& l3Cfg, const LatencyConfig& lat,
                               uint64_t rngSeed)
    : l1(l1Cfg.sets, l1Cfg.ways, l1Cfg.blockBytes, l1Cfg.policy, rngSeed),
      l2(l2Cfg.sets, l2Cfg.ways, l2Cfg.blockBytes, l2Cfg.policy, rngSeed),
      l3(l3Cfg.sets, l3Cfg.ways, l3Cfg.blockBytes, l3Cfg.policy, rngSeed),
      lat_(lat) {}

int CacheHierarchy::access(uint64_t addr, bool isWrite) {
    if (l1.access(addr, isWrite)) return kL1;
    if (l2.access(addr, isWrite)) return kL2;
    if (l3.access(addr, isWrite)) return kL3;
    return kRam;
}

int CacheHierarchy::latencyOf(int level) const {
    switch (level) {
        case kL1: return lat_.l1;
        case kL2: return lat_.l2;
        case kL3: return lat_.l3;
        case kRam: return lat_.ram;
        default: throw std::invalid_argument("no such cache level");
    }
}

double CacheHierarchy::amat() const {
    const double mr1 = 1.0 - l1.hitRate();
    const double mr2 = 1.0 - l2.hitRate();
    const double mr3 = 1.0 - l3.hitRate();
    return lat_.l1 + mr1 * (lat_.l2 + mr2 * (lat_.l3 + mr3 * lat_.ram));
}

void CacheHierarchy::printStats() const {
    const auto printLevel = [](const char* name, const Cache& c) {
        const uint64_t total = c.stats.hits + c.stats.misses;
        std::cout << name << "\n"
                  << "  Accesses:   " << total << "\n"
                  << "  Hits:       " << c.stats.hits << "\n"
                  << "  Misses:     " << c.stats.misses << "\n"
                  << "  Evictions:  " << c.stats.evictions << "\n"
                  << "  Writebacks: " << c.stats.writebacks << "\n"
                  << "  Hit rate:   " << c.hitRate() * 100 << "%\n";
    };
    printLevel("L1", l1);
    printLevel("L2", l2);
    printLevel("L3", l3);
    std::cout << "AMAT: " << amat() << " cycles\n";
}
