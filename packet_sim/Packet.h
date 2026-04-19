#ifndef PACKET_H
#define PACKET_H
#include <cstdint>
#include <cstring>

static const int MAX_PACKET_SIZE = 1536;

struct Packet {
    uint32_t sequenceNumber;
    uint64_t arrivalCycle;
    uint16_t size;
    uint8_t  payload[MAX_PACKET_SIZE];

    Packet(uint32_t seq, uint64_t arrival, uint16_t packetSize)
        : sequenceNumber(seq), arrivalCycle(arrival), size(packetSize)
    {
        memset(payload, seq & 0xFF, MAX_PACKET_SIZE);
    }

    Packet() : sequenceNumber(0), arrivalCycle(0), size(64) {
        memset(payload, 0, MAX_PACKET_SIZE);
    }
};

#endif // PACKET_H