// TraceGenerator.cpp
#include <iostream>
#include <fstream>
#include <cstdlib>    // rand(), srand()
#include <ctime>      // time()
#include <string>
#include <vector>

// We'll write every generated access through this function.
// It picks L or S randomly (70/30 split) and writes the line.
void writeAccess(std::ofstream& file, uint64_t addr) {
    char op = (rand() % 10 < 7) ? 'L' : 'S';
    file << " " << op << " 0x";

    // Write address as hex with leading zeros to 6 digits
    file << std::hex << addr << std::dec;
    file << ",4\n";
}

// --- Pattern 1: Sequential ---
// Accesses addresses 0, B, 2B, 3B, ... one block at a time.
// count = number of accesses to generate
// blockSize = how many bytes per block (should match simulator's B)
void generateSequential(std::ofstream& file, int count, int blockSize) {
    for (int i = 0; i < count; ++i) {
        uint64_t addr = (uint64_t)i * blockSize;
        writeAccess(file, addr);
    }
}

// --- Pattern 2: Strided ---
// Accesses addresses 0, stride, 2*stride, 3*stride, ...
// When stride > cache size this defeats the cache entirely.
void generateStrided(std::ofstream& file, int count, int stride) {
    for (int i = 0; i < count; ++i) {
        uint64_t addr = (uint64_t)i * stride;
        writeAccess(file, addr);
    }
}

// --- Pattern 3: Random ---
// Addresses chosen uniformly at random from [0, addressSpaceSize).
// addressSpaceSize should be much larger than the cache to ensure
// most accesses miss.
void generateRandom(std::ofstream& file, int count, int addressSpaceSize, int blockSize) {
    for (int i = 0; i < count; ++i) {
        // Pick a random block, then align to block boundary.
        // Aligning means we always access the start of a block,
        // which is what a real cache would do.
        int block = rand() % (addressSpaceSize / blockSize);
        uint64_t addr = (uint64_t)block * blockSize;
        writeAccess(file, addr);
    }
}

// --- Pattern 4: Working set loop ---
// Repeatedly cycles through a fixed set of addresses.
// workingSetSize = number of BLOCKS in the working set.
// If workingSetSize * blockSize fits in the cache, hit rates are high.
// If it doesn't fit, you get thrashing.
void generateWorkingSet(std::ofstream& file, int count, int workingSetSize, int blockSize) {
    // Build the working set — a fixed list of addresses
    std::vector<uint64_t> workingSet;
    for (int i = 0; i < workingSetSize; ++i) {
        workingSet.push_back((uint64_t)i * blockSize);
    }

    // Cycle through it repeatedly
    for (int i = 0; i < count; ++i) {
        uint64_t addr = workingSet[i % workingSetSize];
        writeAccess(file, addr);
    }
}

// --- Pattern 5: Mixed ---
// 80% sequential, 20% random.
// Models a realistic workload with mostly local but some unpredictable access.
void generateMixed(std::ofstream& file, int count, int blockSize, int addressSpaceSize) {
    int seqCounter = 0;   // tracks where we are in the sequential stream

    for (int i = 0; i < count; ++i) {
        if (rand() % 10 < 8) {
            // 80%: sequential
            uint64_t addr = (uint64_t)seqCounter * blockSize;
            writeAccess(file, addr);
            seqCounter++;
        } else {
            // 20%: random
            int block = rand() % (addressSpaceSize / blockSize);
            uint64_t addr = (uint64_t)block * blockSize;
            writeAccess(file, addr);
        }
    }
}

// --- main ---
int main() {
    srand(time(nullptr));
    // time(nullptr) returns the current time as an integer.
    // Passing it to srand() seeds the random number generator so
    // you get different random sequences each run.
    // Without this, rand() produces the same sequence every time.

    int count     = 10000;   // accesses per trace
    int blockSize = 16;      // must match simulator's B
    int stride    = 64;      // for strided pattern
    int wsSize    = 12;       // working set: 8 blocks
    int addrSpace = 1 << 16; // 64KB address space for random pattern

    // Generate one file per pattern
    // We'll run each through the simulator separately

    {
        std::ofstream f("trace_sequential.txt");
        generateSequential(f, count, blockSize);
        std::cout << "Generated trace_sequential.txt\n";
    }
    {
        std::ofstream f("trace_strided.txt");
        generateStrided(f, count, stride);
        std::cout << "Generated trace_strided.txt\n";
    }
    {
        std::ofstream f("trace_random.txt");
        generateRandom(f, count, addrSpace, blockSize);
        std::cout << "Generated trace_random.txt\n";
    }
    {
        std::ofstream f("trace_workingset.txt");
        generateWorkingSet(f, count, wsSize, blockSize);
        std::cout << "Generated trace_workingset.txt\n";
    }
    {
        std::ofstream f("trace_mixed.txt");
        generateMixed(f, count, blockSize, addrSpace);
        std::cout << "Generated trace_mixed.txt\n";
    }

    return 0;
}