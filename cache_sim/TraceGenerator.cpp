// Generates synthetic memory-access traces in a valgrind-lackey-like format:
//   " L 0x<hex addr>,<size>"  (load)   " S 0x<hex addr>,<size>"  (store)
//
// Every pattern is deterministic under --seed (default 42), so committed
// result tables can be regenerated exactly.
#include <cstdint>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

struct Options {
    std::string pattern;
    std::string outFile;
    int count = 10000;
    int blockBytes = 16;
    int stride = 64;
    int workingSetBlocks = 12;
    int addressSpace = 1 << 16;
    uint64_t seed = 42;
};

class TraceWriter {
public:
    TraceWriter(std::ofstream& out, uint64_t seed) : out_(out), rng_(seed), opPick_(0, 9) {}

    void write(uint64_t addr) {
        const char op = opPick_(rng_) < 7 ? 'L' : 'S';  // 70/30 load/store mix
        out_ << ' ' << op << " 0x" << std::hex << addr << std::dec << ",4\n";
    }

    std::mt19937_64& rng() { return rng_; }

private:
    std::ofstream& out_;
    std::mt19937_64 rng_;
    std::uniform_int_distribution<int> opPick_;
};

void generateSequential(TraceWriter& w, const Options& o) {
    for (int i = 0; i < o.count; ++i) w.write(static_cast<uint64_t>(i) * o.blockBytes);
}

void generateStrided(TraceWriter& w, const Options& o) {
    for (int i = 0; i < o.count; ++i) w.write(static_cast<uint64_t>(i) * o.stride);
}

void generateRandom(TraceWriter& w, const Options& o) {
    std::uniform_int_distribution<int> blockPick(0, o.addressSpace / o.blockBytes - 1);
    for (int i = 0; i < o.count; ++i)
        w.write(static_cast<uint64_t>(blockPick(w.rng())) * o.blockBytes);
}

void generateWorkingSet(TraceWriter& w, const Options& o) {
    for (int i = 0; i < o.count; ++i)
        w.write(static_cast<uint64_t>(i % o.workingSetBlocks) * o.blockBytes);
}

void generateMixed(TraceWriter& w, const Options& o) {
    std::uniform_int_distribution<int> pathPick(0, 9);
    std::uniform_int_distribution<int> blockPick(0, o.addressSpace / o.blockBytes - 1);
    uint64_t seq = 0;
    for (int i = 0; i < o.count; ++i) {
        if (pathPick(w.rng()) < 8)
            w.write(seq++ * o.blockBytes);  // 80% sequential
        else
            w.write(static_cast<uint64_t>(blockPick(w.rng())) * o.blockBytes);
    }
}

int generate(const Options& o) {
    std::ofstream out(o.outFile);
    if (!out) {
        std::cerr << "error: cannot open output file: " << o.outFile << "\n";
        return 1;
    }
    TraceWriter w(out, o.seed);
    if (o.pattern == "sequential") generateSequential(w, o);
    else if (o.pattern == "strided") generateStrided(w, o);
    else if (o.pattern == "random") generateRandom(w, o);
    else if (o.pattern == "workingset") generateWorkingSet(w, o);
    else if (o.pattern == "mixed") generateMixed(w, o);
    else {
        std::cerr << "error: unknown pattern '" << o.pattern
                  << "' (sequential|strided|random|workingset|mixed)\n";
        return 1;
    }
    std::cout << "wrote " << o.count << " accesses (" << o.pattern << ") to " << o.outFile
              << "\n";
    return 0;
}

int usage() {
    std::cerr <<
        "Usage: tracegen <pattern> <outputFile> [options]\n"
        "       tracegen --all [directory]\n\n"
        "Patterns: sequential | strided | random | workingset | mixed\n"
        "Options:\n"
        "  --count N        accesses to generate (default 10000)\n"
        "  --block N        block size in bytes, power of two (default 16)\n"
        "  --stride N       stride in bytes for 'strided' (default 64)\n"
        "  --ws-blocks N    working-set size in blocks for 'workingset' (default 12)\n"
        "  --addr-space N   address space in bytes for random draws (default 65536)\n"
        "  --seed N         RNG seed (default 42)\n\n"
        "--all emits the five standard experiment traces (seed 42) plus the\n"
        "8-block 'fits in L1' working-set variant used in the writeup.\n";
    return 1;
}

bool parseInt(const std::string& s, int& out) {
    try {
        size_t pos = 0;
        const long v = std::stol(s, &pos);
        if (pos != s.size() || v <= 0 || v > INT32_MAX) return false;
        out = static_cast<int>(v);
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) return usage();

    if (args[0] == "--all") {
        const std::string dir = args.size() > 1 ? args[1] + "/" : "";
        Options o;
        for (const char* p : {"sequential", "strided", "random", "mixed"}) {
            o.pattern = p;
            o.outFile = dir + "trace_" + p + ".txt";
            if (generate(o) != 0) return 1;
        }
        o.pattern = "workingset";
        o.workingSetBlocks = 12;  // overflows the 8-line experimental L1 by 4
        o.outFile = dir + "trace_workingset.txt";
        if (generate(o) != 0) return 1;
        o.workingSetBlocks = 8;  // exactly fills the experimental L1
        o.outFile = dir + "trace_workingset_fits.txt";
        return generate(o);
    }

    if (args.size() < 2) return usage();
    Options o;
    o.pattern = args[0];
    o.outFile = args[1];
    for (size_t i = 2; i < args.size(); i += 2) {
        if (i + 1 >= args.size()) return usage();
        const std::string& flag = args[i];
        const std::string& val = args[i + 1];
        bool ok = true;
        if (flag == "--count") ok = parseInt(val, o.count);
        else if (flag == "--block") ok = parseInt(val, o.blockBytes);
        else if (flag == "--stride") ok = parseInt(val, o.stride);
        else if (flag == "--ws-blocks") ok = parseInt(val, o.workingSetBlocks);
        else if (flag == "--addr-space") ok = parseInt(val, o.addressSpace);
        else if (flag == "--seed") {
            try { o.seed = std::stoull(val); } catch (...) { ok = false; }
        } else return usage();
        if (!ok) {
            std::cerr << "error: bad value for " << flag << ": " << val << "\n";
            return 1;
        }
    }
    return generate(o);
}
