#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>

#include "Packet.h"
#include "RingBuffer.h"
#include "PacketProcessor.h"
#include "../cache_sim/CacheHierarchy.h"

struct TraceEntry
{
    uint64_t arrivalCycle;
    uint16_t size;
};

std::vector<TraceEntry> loadTrace(const std::string &filename)
{
    std::vector<TraceEntry> entries;
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Error: could not open trace file: " << filename << "\n";
        return entries;
    }
    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty())
            continue;
        std::istringstream ss(line);
        TraceEntry e;
        ss >> e.arrivalCycle >> e.size;
        entries.push_back(e);
    }
    return entries;
}

std::string cacheLevel(size_t bytes)
{
    if (bytes <= 32 * 1024)
        return "L1";
    if (bytes <= 256 * 1024)
        return "L2";
    if (bytes <= 8 * 1024 * 1024)
        return "L3";
    return "RAM";
}

CacheHierarchy makeCacheHierarchy()
{
    return CacheHierarchy(
        64, 8, 64, "lru",   // L1: 32KB
        512, 8, 64, "lru",  // L2: 256KB
        8192, 16, 64, "lru" // L3: 8MB
    );
}

void runExperiment(const std::vector<TraceEntry> &trace, int bufferSlots)
{
    CacheHierarchy cache = makeCacheHierarchy();
    RingBuffer buffer(bufferSlots);
    PacketProcessor processor(cache, 64, bufferSlots);

    size_t footprint = buffer.footprintBytes();
    std::cout << "=== Ring buffer: " << bufferSlots << " slots ("
              << footprint << " bytes) | fits in: " << cacheLevel(footprint) << " ===\n";

    uint64_t currentCycle = 0;
    size_t nextEntry = 0;

    while (nextEntry < trace.size() || !buffer.isEmpty())
    {
        while (nextEntry < trace.size() &&
               trace[nextEntry].arrivalCycle <= currentCycle)
        {
            const TraceEntry &e = trace[nextEntry];
            Packet pkt(nextEntry, currentCycle, e.size); // use currentCycle, not e.arrivalCycle
            buffer.enqueue(pkt);
            nextEntry++;
        }

        int cost = processor.tick(buffer, currentCycle);
        currentCycle += cost;
    }

    buffer.stats.totalCycles = currentCycle;

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Packets arrived:   " << buffer.stats.totalArrived << "\n";
    std::cout << "Packets processed: " << buffer.stats.totalProcessed << "\n";
    std::cout << "Packets dropped:   " << buffer.stats.totalDropped << "\n";
    std::cout << "Drop rate:         " << buffer.stats.dropRate() * 100 << "%\n";
    std::cout << "Throughput:        " << buffer.stats.throughput() << " packets/cycle\n";
    std::cout << "Avg latency:       " << std::fixed << std::setprecision(2)
              << buffer.stats.avgLatency() << " cycles/packet\n";
    std::cout << "Cache hit rate:    " << processor.stats.cacheHitRate() * 100 << "%\n";
    std::cout << "AMAT:              " << cache.amat() << " cycles\n\n";
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: packet_sim <traceFile>\n";
        return 1;
    }

    std::vector<TraceEntry> trace = loadTrace(argv[1]);
    if (trace.empty())
    {
        std::cerr << "Error: trace file is empty or could not be read.\n";
        return 1;
    }

    std::cout << "Loaded " << trace.size() << " trace entries from " << argv[1] << "\n\n";

    runExperiment(trace, 8);
    runExperiment(trace, 100);
    runExperiment(trace, 1000);
    runExperiment(trace, 6000);
}