// Packet buffer experiment driver.
//
// Replays a packet-arrival trace against a ring buffer of each configured
// size, with the cache hierarchy as the timing model, and reports drop rate,
// latency, throughput, and where in the hierarchy accesses were served.
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../cache_sim/CacheHierarchy.h"
#include "Packet.h"
#include "PacketProcessor.h"
#include "RingBuffer.h"

namespace {

constexpr int kCacheBlockBytes = 64;
const std::vector<int> kDefaultBufferSlots = {8, 100, 1000, 6000};

struct TraceEntry {
    uint64_t arrivalCycle;
    uint16_t size;
};

CacheHierarchy makeHierarchy() {
    return CacheHierarchy({64, 8, kCacheBlockBytes, "lru"},     // L1: 32 KB
                          {512, 8, kCacheBlockBytes, "lru"},    // L2: 256 KB
                          {8192, 16, kCacheBlockBytes, "lru"}); // L3: 8 MB
}

// Label derived from the actual configured capacities so it can never drift
// from makeHierarchy().
std::string residencyLabel(int64_t bytes, const CacheHierarchy& h) {
    if (bytes <= h.l1.capacityBytes()) return "L1";
    if (bytes <= h.l2.capacityBytes()) return "L2";
    if (bytes <= h.l3.capacityBytes()) return "L3";
    return "RAM";
}

std::vector<TraceEntry> loadTrace(const std::string& filename) {
    std::vector<TraceEntry> entries;
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "error: cannot open trace: " << filename << "\n";
        return entries;
    }
    std::string line;
    uint64_t malformed = 0;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        TraceEntry e{};
        if (!(ss >> e.arrivalCycle >> e.size) || e.size == 0 ||
            e.size > kMaxPacketBytes) {
            ++malformed;
            continue;
        }
        entries.push_back(e);
    }
    if (malformed > 0)
        std::cerr << "warning: skipped " << malformed << " malformed lines in " << filename
                  << "\n";
    return entries;
}

void runExperiment(const std::vector<TraceEntry>& trace, int bufferSlots) {
    CacheHierarchy cache = makeHierarchy();
    RingBuffer buffer(bufferSlots);
    PacketProcessor processor(cache, kCacheBlockBytes);

    const int64_t footprint = buffer.footprintBytes();
    std::cout << "=== Ring buffer: " << bufferSlots << " slots (" << footprint
              << " bytes) | fits in: " << residencyLabel(footprint, cache) << " ===\n";

    uint64_t currentCycle = 0;
    size_t next = 0;

    while (next < trace.size() || !buffer.isEmpty() || processor.busy()) {
        // Nothing in flight and nothing queued: jump straight to the next
        // arrival instead of burning one idle tick per cycle.
        if (!processor.busy() && buffer.isEmpty() && next < trace.size() &&
            trace[next].arrivalCycle > currentCycle) {
            processor.stats.idleCycles += trace[next].arrivalCycle - currentCycle;
            currentCycle = trace[next].arrivalCycle;
        }

        while (next < trace.size() && trace[next].arrivalCycle <= currentCycle) {
            // Latency is measured from the trace arrival time, so waiting
            // caused by a stalled processor counts against the buffer.
            buffer.enqueue(
                Packet(static_cast<uint32_t>(next), trace[next].arrivalCycle,
                       trace[next].size));
            ++next;
        }

        currentCycle += processor.tick(buffer, currentCycle);
    }

    const ProcessorStats& p = processor.stats;
    const RingBufferStats& b = buffer.stats;
    const uint64_t served = p.hitsAtLevel[1] + p.hitsAtLevel[2] + p.hitsAtLevel[3] +
                            p.hitsAtLevel[4];

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Packets arrived:   " << b.totalArrived << "\n";
    std::cout << "Packets processed: " << p.packetsProcessed << "\n";
    std::cout << "Packets dropped:   " << b.totalDropped << "\n";
    std::cout << "Drop rate:         " << b.dropRate() * 100 << "%\n";
    std::cout << "Throughput:        "
              << (currentCycle == 0
                      ? 0.0
                      : static_cast<double>(p.packetsProcessed) / currentCycle)
              << " packets/cycle\n";
    std::cout << "Avg latency:       " << std::setprecision(2) << p.avgLatency()
              << " cycles/packet\n";
    std::cout << "Served from:       ";
    const char* names[] = {"", "L1", "L2", "L3", "RAM"};
    for (int lvl = CacheHierarchy::kL1; lvl <= CacheHierarchy::kRam; ++lvl) {
        std::cout << names[lvl] << " "
                  << (served == 0 ? 0.0 : 100.0 * p.hitsAtLevel[lvl] / served) << "%"
                  << (lvl < CacheHierarchy::kRam ? " | " : "\n");
    }
    std::cout << "AMAT:              " << std::setprecision(2) << cache.amat()
              << " cycles\n\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: packet_sim <traceFile> [bufferSlots...]\n";
        return 1;
    }

    const std::vector<TraceEntry> trace = loadTrace(argv[1]);
    if (trace.empty()) {
        std::cerr << "error: trace is empty or unreadable\n";
        return 1;
    }
    std::cout << "Loaded " << trace.size() << " trace entries from " << argv[1] << "\n\n";

    std::vector<int> sizes;
    for (int i = 2; i < argc; ++i) {
        try {
            const int v = std::stoi(argv[i]);
            if (v <= 0) throw std::invalid_argument("");
            sizes.push_back(v);
        } catch (...) {
            std::cerr << "error: buffer sizes must be positive integers\n";
            return 1;
        }
    }
    if (sizes.empty()) sizes = kDefaultBufferSlots;

    for (const int slots : sizes) runExperiment(trace, slots);
    return 0;
}
