#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

// Define the packet structure
struct Packet
{
    uint8_t version;
    uint8_t type;
    uint32_t seq_num;
    uint32_t ack_num;
    uint16_t payload_length;
    uint8_t flags;
    uint16_t checksum;
    char payload[1024]; // Example payload size
};

// Function to calculate a simple checksum
uint16_t calculate_checksum(const Packet &packet)
{
    uint16_t sum = 0;
    sum += packet.version;
    sum += packet.type;
    sum += packet.seq_num & 0xFFFF;
    sum += (packet.seq_num >> 16) & 0xFFFF;
    sum += packet.ack_num & 0xFFFF;
    sum += (packet.ack_num >> 16) & 0xFFFF;
    sum += packet.payload_length;
    sum += packet.flags;
    for (size_t i = 0; i < packet.payload_length; ++i)
    {
        sum += packet.payload[i];
    }
    return ~sum;
}

// Function to send a packet
bool send_packet(int sock, const sockaddr_in &dest_addr, const Packet &packet)
{
    int sendResult = sendto(sock, &packet, sizeof(packet), 0, (const sockaddr *)&dest_addr, sizeof(dest_addr));
    if (sendResult < 0)
    {
        std::cerr << "Error sending data" << std::endl;
        return false;
    }
    std::cout << "Sent packet with payload: " << packet.payload << std::endl;
    return true;
}

// Function to receive a packet
bool receive_packet(int sock, Packet &packet, sockaddr_in &src_addr)
{
    socklen_t src_addr_len = sizeof(src_addr);
    int bytesReceived = recvfrom(sock, &packet, sizeof(packet), 0, (sockaddr *)&src_addr, &src_addr_len);
    if (bytesReceived < 0)
    {
        std::cerr << "Error receiving data" << std::endl;
        return false;
    }

    // Check the checksum of the received packet
    if (packet.checksum == calculate_checksum(packet))
    {
        char src_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &src_addr.sin_addr, src_ip, INET_ADDRSTRLEN);
        std::cout << "Received valid packet from " << src_ip << ":" << ntohs(src_addr.sin_port) << " with payload: " << packet.payload << std::endl;
        return true;
    }
    else
    {
        std::cerr << "Checksum error!" << std::endl;
        return false;
    }
}

int main()
{
    // Create a UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        std::cerr << "Error creating socket" << std::endl;
        return 1;
    }

    // Set up the address for sending and receiving
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(54000);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    // Bind the socket to the port
    if (bind(sock, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        std::cerr << "Error binding socket" << std::endl;
        close(sock);
        return 1;
    }

    // Set up the address structure for sending messages
    sockaddr_in sendAddr;
    sendAddr.sin_family = AF_INET;
    sendAddr.sin_port = htons(54001); // Use a different port for sending
    inet_pton(AF_INET, "127.0.0.1", &sendAddr.sin_addr);

    // Sequence number for outgoing packets
    uint32_t seq_num = 0;

    // Loop to send and receive packets
    while (true)
    {
        // Create a packet to send
        Packet packet;
        packet.version = 1;
        packet.type = 1; // Type 1 for data packet
        packet.seq_num = seq_num++;
        packet.ack_num = 0;
        const char *msg = "Hello from sender!";
        strncpy(packet.payload, msg, sizeof(packet.payload));
        packet.payload_length = strlen(msg);
        packet.flags = 0;
        packet.checksum = calculate_checksum(packet);

        // Send the packet
        if (!send_packet(sock, sendAddr, packet))
        {
            close(sock);
            return 1;
        }

        // Receive a packet
        Packet recvPacket;
        sockaddr_in recvAddr;
        if (!receive_packet(sock, recvPacket, recvAddr))
        {
            continue;
        }

        // Pause before the next iteration
        sleep(1);
    }

    close(sock);
    return 0;
}
