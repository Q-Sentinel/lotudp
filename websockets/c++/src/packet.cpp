#include "packet.h"
#include <iostream>
uint16_t calculate_checksum(const Packet &packet)
{
    std::cout << "Calculating checksum" << std::endl;
    uint16_t sum = 0;
    sum += packet.version;
    sum += packet.type;

    sum += packet.payload_length;
    sum += packet.flags;
    for (size_t i = 0; i < sizeof(packet.topic); ++i)
    {
        sum += packet.topic[i];
    }
    for (size_t i = 0; i < packet.payload_length; ++i)
    {
        sum += packet.payload[i];
    }
    std::cout << "Checksum calculated" << static_cast<uint16_t>(~sum) << std::endl;
    return static_cast<uint16_t>(~sum);
}
