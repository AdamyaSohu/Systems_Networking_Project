// Generates packet-arrival traces: one "<arrivalCycle> <sizeBytes>" line per
// packet. All patterns are deterministic, so traces are never committed;
// they are regenerated exactly from the commands in the README.
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

namespace {

constexpr int kSmallPacket = 64;
constexpr int kLargePacket = 1536;

void generateConstant(std::ofstream& out, int numPackets, int gapCycles) {
    for (int i = 0; i < numPackets; ++i)
        out << static_cast<uint64_t>(i + 1) * gapCycles << " " << kSmallPacket << "\n";
}

void generateBursty(std::ofstream& out, int numPackets, int gapCycles, int burstSize) {
    int written = 0;
    for (int burst = 0; written < numPackets; ++burst) {
        const uint64_t start = static_cast<uint64_t>(burst + 1) * gapCycles;
        for (int i = 0; i < burstSize && written < numPackets; ++i, ++written)
            out << start << " " << kSmallPacket << "\n";
    }
}

void generateMixed(std::ofstream& out, int numPackets, int gapCycles) {
    for (int i = 0; i < numPackets; ++i) {
        const int size = (i % 2 == 0) ? kSmallPacket : kLargePacket;
        out << static_cast<uint64_t>(i + 1) * gapCycles << " " << size << "\n";
    }
}

int usage() {
    std::cerr << "Usage: packetgen <pattern> <numPackets> <gapCycles> <outputFile>"
                 " [burstSize]\n"
                 "  pattern: constant | bursty | mixed   (burstSize default 8)\n";
    return 1;
}

bool parsePositive(const char* s, int& out) {
    try {
        size_t pos = 0;
        const long v = std::stol(s, &pos);
        if (s[pos] != '\0' || v <= 0 || v > INT32_MAX) return false;
        out = static_cast<int>(v);
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 5) return usage();

    const std::string pattern = argv[1];
    int numPackets = 0;
    int gapCycles = 0;
    int burstSize = 8;
    if (!parsePositive(argv[2], numPackets) || !parsePositive(argv[3], gapCycles) ||
        (argc >= 6 && !parsePositive(argv[5], burstSize))) {
        std::cerr << "error: counts, cycles, and burst size must be positive integers\n";
        return 1;
    }

    std::ofstream out(argv[4]);
    if (!out) {
        std::cerr << "error: cannot open output file: " << argv[4] << "\n";
        return 1;
    }

    if (pattern == "constant") generateConstant(out, numPackets, gapCycles);
    else if (pattern == "bursty") generateBursty(out, numPackets, gapCycles, burstSize);
    else if (pattern == "mixed") generateMixed(out, numPackets, gapCycles);
    else return usage();

    std::cout << "wrote " << numPackets << " packets (" << pattern << ") to " << argv[4]
              << "\n";
    return 0;
}
