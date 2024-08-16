#ifndef PACKET_H
#define PACKET_H

#include <cstdint>
#include <cstring>

struct Packet
{
    uint8_t version;
    uint8_t type;
    uint32_t seq_num;
    uint32_t ack_num;
    uint16_t payload_length;
    uint8_t flags;
    uint16_t checksum;
    char topic[32];
    char payload[1024];
} __attribute__((packed));

uint16_t calculate_checksum(const Packet &packet);

#endif // PACKET_H
