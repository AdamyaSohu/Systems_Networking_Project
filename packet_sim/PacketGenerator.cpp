#include <iostream>
#include <fstream>
#include <string>

static const int SMALL_PACKET = 64;
static const int LARGE_PACKET = 1536;

void generateConstant(std::ofstream& out, int numPackets, int gapCycles) {
    for (int i = 0; i < numPackets; ++i) {
        uint64_t arrivalCycle = (uint64_t)(i + 1) * gapCycles;
        out << arrivalCycle << " " << SMALL_PACKET << "\n";
    }
}

void generateBursty(std::ofstream& out, int numPackets, int gapCycles, int burstSize) {
    int packetsWritten = 0;
    int burstNumber    = 0;

    while (packetsWritten < numPackets) {
        uint64_t burstStartCycle = (uint64_t)(burstNumber + 1) * gapCycles;

        for (int i = 0; i < burstSize && packetsWritten < numPackets; ++i) {
            out << burstStartCycle << " " << SMALL_PACKET << "\n";
            packetsWritten++;
        }

        burstNumber++;
    }
}

void generateMixed(std::ofstream& out, int numPackets, int gapCycles) {
    for (int i = 0; i < numPackets; ++i) {
        uint64_t arrivalCycle = (uint64_t)(i + 1) * gapCycles;
        int size = (i % 2 == 0) ? SMALL_PACKET : LARGE_PACKET;
        out << arrivalCycle << " " << size << "\n";
    }
}

int main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << "Usage: packetgen <pattern> <numPackets> <gapCycles> <outputFile> [burstSize]\n";
        std::cerr << "  pattern: constant | bursty | mixed\n";
        return 1;
    }

    std::string pattern = argv[1];
    int numPackets      = std::stoi(argv[2]);
    int gapCycles       = std::stoi(argv[3]);
    std::string outFile = argv[4];
    int burstSize       = (argc >= 6) ? std::stoi(argv[5]) : 8;

    std::ofstream out(outFile);
    if (!out.is_open()) {
        std::cerr << "Error: could not open output file: " << outFile << "\n";
        return 1;
    }

    if (pattern == "constant") {
        generateConstant(out, numPackets, gapCycles);
    } else if (pattern == "bursty") {
        generateBursty(out, numPackets, gapCycles, burstSize);
    } else if (pattern == "mixed") {
        generateMixed(out, numPackets, gapCycles);
    } else {
        std::cerr << "Error: unknown pattern '" << pattern << "'. Use: constant | bursty | mixed\n";
        return 1;
    }

    std::cout << "Wrote " << numPackets << " packets (" << pattern << ") to " << outFile << "\n";
    return 0;
}