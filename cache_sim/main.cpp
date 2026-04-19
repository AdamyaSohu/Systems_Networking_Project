// main.cpp
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "CacheHierarchy.h"

void runTrace(const std::string& filename, CacheHierarchy& hierarchy) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: could not open file: " << filename << "\n";
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == 'I') continue;

        char op = line[1];
        std::istringstream ss(line.substr(3));
        uint64_t addr;
        int size;
        char comma;
        ss >> std::hex >> addr >> comma >> std::dec >> size;

        if      (op == 'L') { hierarchy.access(addr, false); }
        else if (op == 'S') { hierarchy.access(addr, true);  }
        else if (op == 'M') { hierarchy.access(addr, false);
                              hierarchy.access(addr, true);  }
    }
}

void runExperiment(const std::string& traceName, const std::string& policy) {
    CacheHierarchy hierarchy(
        4,  2, 16, policy,
        8,  4, 16, policy,
        16, 8, 16, policy
    );

    runTrace(traceName, hierarchy);

    std::cout << "\n=== " << traceName << " | policy: " << policy << " ===\n";
    hierarchy.printStats();
}

int main() {
    std::vector<std::string> traces = {
        "trace_sequential.txt",
        "trace_strided.txt",
        "trace_random.txt",
        "trace_workingset.txt",
        "trace_mixed.txt"
    };

    std::vector<std::string> policies = { "lru", "fifo", "random" };

    for (const auto& trace : traces) {
        for (const auto& policy : policies) {
            runExperiment(trace, policy);
        }
    }

    return 0;
}