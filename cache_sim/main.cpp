// Cache simulator experiment driver.
//
// Replays one or more memory-access traces through the three-level hierarchy
// under each requested replacement policy and prints per-level statistics.
//
// The default geometry is deliberately small (L1 = 4 sets x 2 ways x 16 B =
// 128 B). The committed traces are 10,000 accesses; a realistically sized L1
// would never see capacity pressure at that scale, and capacity pressure is
// the thing under study. --config realistic switches to 32 KB / 256 KB / 8 MB.
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "CacheHierarchy.h"

namespace {

struct HierarchyGeometry {
    LevelConfig l1, l2, l3;
};

HierarchyGeometry smallGeometry(const std::string& policy) {
    return {{4, 2, 16, policy}, {8, 4, 16, policy}, {16, 8, 16, policy}};
}

HierarchyGeometry realisticGeometry(const std::string& policy) {
    return {{64, 8, 64, policy}, {512, 8, 64, policy}, {8192, 16, 64, policy}};
}

// Trace lines look like " L 0x1f2a0,4". Ops: L load, S store, M modify
// (load + store). Instruction-fetch lines ("I ...") and malformed lines are
// skipped; malformed lines are counted and reported so silent trace damage
// is visible.
bool runTrace(const std::string& filename, CacheHierarchy& hierarchy) {
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "error: cannot open trace: " << filename << "\n";
        return false;
    }

    uint64_t malformed = 0;
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        char op = 0;
        std::string addrField;
        if (!(ss >> op >> addrField)) continue;  // blank line
        if (op == 'I') continue;

        uint64_t addr = 0;
        try {
            addr = std::stoull(addrField, nullptr, 16);
        } catch (...) {
            ++malformed;
            continue;
        }

        switch (op) {
            case 'L': hierarchy.access(addr, false); break;
            case 'S': hierarchy.access(addr, true); break;
            case 'M':
                hierarchy.access(addr, false);
                hierarchy.access(addr, true);
                break;
            default: ++malformed;
        }
    }
    if (malformed > 0)
        std::cerr << "warning: skipped " << malformed << " malformed lines in " << filename
                  << "\n";
    return true;
}

int usage() {
    std::cerr << "Usage: cache_sim <trace> [trace...] [--policy lru|fifo|random|all]\n"
                 "                 [--config small|realistic] [--seed N]\n";
    return 1;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::vector<std::string> traces;
    std::vector<std::string> policies = {"lru", "fifo", "random"};
    bool realistic = false;
    uint64_t seed = Cache::kDefaultSeed;

    std::vector<std::string> args(argv + 1, argv + argc);
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--policy") {
            if (++i >= args.size()) return usage();
            if (args[i] != "all") policies = {args[i]};
        } else if (args[i] == "--config") {
            if (++i >= args.size()) return usage();
            if (args[i] == "realistic") realistic = true;
            else if (args[i] != "small") return usage();
        } else if (args[i] == "--seed") {
            if (++i >= args.size()) return usage();
            try { seed = std::stoull(args[i]); } catch (...) { return usage(); }
        } else if (!args[i].empty() && args[i][0] == '-') {
            return usage();
        } else {
            traces.push_back(args[i]);
        }
    }
    if (traces.empty()) return usage();

    for (const auto& trace : traces) {
        for (const auto& policy : policies) {
            const HierarchyGeometry g =
                realistic ? realisticGeometry(policy) : smallGeometry(policy);
            CacheHierarchy hierarchy(g.l1, g.l2, g.l3, {}, seed);
            if (!runTrace(trace, hierarchy)) return 1;
            std::cout << "\n=== " << trace << " | policy: " << policy << " ===\n";
            hierarchy.printStats();
        }
    }
    return 0;
}
