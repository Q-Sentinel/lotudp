#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <queue>
#include <mutex>
#include <condition_variable>
const int SEND_DELAY_S = 10;
const int TIMEOUT_MS = 10000; // Timeout for retransmission in milliseconds

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

// Shared resources for acknowledgment handling
std::queue<uint32_t> ack_queue;
std::mutex ack_mutex;
std::condition_variable ack_cv;

// Function to receive packets in a separate thread
void receive_packets(int sock, uint32_t &expected_seq_num)
{
    while (true)
    {
        Packet recvPacket;
        sockaddr_in recvAddr;
        socklen_t recvAddrLen = sizeof(recvAddr);
        int bytesReceived = recvfrom(sock, &recvPacket, sizeof(recvPacket), 0, (sockaddr *)&recvAddr, &recvAddrLen);
        if (bytesReceived < 0)
        {
            std::cerr << "Error receiving data" << std::endl;
            continue;
        }

        // Check the checksum of the received packet
        if (recvPacket.checksum == calculate_checksum(recvPacket))
        {
            char src_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &recvAddr.sin_addr, src_ip, INET_ADDRSTRLEN);
            std::cout << "Received valid packet from " << src_ip << ":" << ntohs(recvAddr.sin_port) << " with payload: " << recvPacket.payload << std::endl;

            // Handle acknowledgment
            if (recvPacket.type == 1) // Type 1 for data packet
            {
                // Send an acknowledgment
                Packet ackPacket;
                ackPacket.version = 1;
                ackPacket.type = 2; // Type 2 for acknowledgment
                ackPacket.seq_num = 0;
                ackPacket.ack_num = recvPacket.seq_num;
                ackPacket.payload_length = 0;
                ackPacket.flags = 0;
                ackPacket.checksum = calculate_checksum(ackPacket);
                send_packet(sock, recvAddr, ackPacket);
                std::cout << "Sent acknowledgment for sequence number: " << recvPacket.seq_num << std::endl;
            }

            // Notify sender about the received acknowledgment
            if (recvPacket.type == 2)
            {
                std::lock_guard<std::mutex> lock(ack_mutex);
                ack_queue.push(recvPacket.ack_num);
            }
            ack_cv.notify_one();

            // Update expected sequence number
            expected_seq_num = recvPacket.seq_num + 1;
        }
        else
        {
            std::cerr << "Checksum error!" << std::endl;
        }
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

    // Start the thread for receiving packets
    uint32_t expected_seq_num = 0;
    std::thread recv_thread(receive_packets, sock, std::ref(expected_seq_num));

    // Sequence number for outgoing packets
    uint32_t seq_num = 0;

    // Loop to send packets
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
            recv_thread.join(); // Wait for the receive thread to finish
            return 1;
        }

        // Wait for acknowledgment with retransmission logic
        auto start = std::chrono::steady_clock::now();
        bool ack_received = false;

        std::unique_lock<std::mutex> lock(ack_mutex);
        while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(TIMEOUT_MS))
        {
            // Check for acknowledgment in the queue
            if (!ack_queue.empty())
            {
                uint32_t ack_num = ack_queue.front();
                ack_queue.pop();
                if (ack_num == packet.seq_num)
                {
                    ack_received = true;
                    break;
                }
            }
            ack_cv.wait_for(lock, std::chrono::milliseconds(10));
        }

        if (!ack_received)
        {
            std::cerr << "No acknowledgment received, retransmitting..." << std::endl;
            continue; // Retransmit
        }
        else
        {
            std::cout << "Ack received" << std::endl;
        }
        // Pause before the next iteration
        std::this_thread::sleep_for(std::chrono::seconds(SEND_DELAY_S));
    }

    // Clean up and close the socket
    close(sock);
    recv_thread.join(); // Wait for the receive thread to finish
    return 0;
}
