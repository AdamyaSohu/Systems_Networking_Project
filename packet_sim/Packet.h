#ifndef PACKET_H
#define PACKET_H

#include <cstdint>
#include <cstring>

inline constexpr int kMaxPacketBytes = 1536;  // Ethernet MTU payload

struct Packet {
    uint32_t sequenceNumber = 0;
    uint64_t arrivalCycle = 0;  // arrival time from the trace, not enqueue time
    uint16_t size = 64;
    uint8_t payload[kMaxPacketBytes] = {};

    Packet() = default;
    Packet(uint32_t seq, uint64_t arrival, uint16_t bytes)
        : sequenceNumber(seq), arrivalCycle(arrival), size(bytes) {
        std::memset(payload, seq & 0xFF, sizeof(payload));
    }
};

// The published working-set tables (12,480 B for 8 slots, etc.) assume this
// exact layout. If padding rules ever change the size, fail the build rather
// than silently shifting every footprint in the results.
static_assert(sizeof(Packet) == 1560, "Packet layout changed; results tables depend on it");

#endif  // PACKET_H
