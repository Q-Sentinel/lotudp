#include "socket_utils.h"

#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <vector>

extern std::queue<uint32_t> ack_queue;
extern std::mutex ack_mutex;
extern std::condition_variable ack_cv;

bool send_packet(int sock, const sockaddr_in &dest_addr, Packet &packet)
{
    // Send the packet
    int sendResult = sendto(sock, &packet, sizeof(packet), 0, (const sockaddr *)&dest_addr, sizeof(dest_addr));
    if (sendResult < 0)
    {
        std::cerr << "Error sending data" << std::endl;
        return false;
    }
    std::cout << "Sent packet with payload: " << packet.payload << std::endl;

    // Wait for acknowledgment
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
        return false; // Indicate that retransmission is needed
    }
    else
    {
        std::cout << "Ack received" << std::endl;
        return true; // Acknowledgment received
    }
}

bool create_and_send_packet(int sock, const sockaddr_in &dest_addr, uint32_t &seq_num,
                            const std::vector<std::string> &keys,
                            const std::vector<std::string> &values,
                            const std::vector<uint8_t> &data_types, const std::string &topic)
{

    if (keys.size() != values.size() || keys.size() != data_types.size())
    {
        std::cerr << "Mismatch in size of keys, values, or data types" << std::endl;
        return false;
    }

    // Create a packet to send
    Packet packet;
    packet.version = 1;
    packet.type = 1; // Type 1 for data packet
    packet.seq_num = seq_num++;
    packet.ack_num = 0;

    size_t offset = 0;

    strncpy(packet.topic, topic.c_str(), sizeof(packet.topic));
    packet.topic[sizeof(packet.topic) - 1] = '\0';
    // Prepare the payload
    for (size_t i = 0; i < keys.size(); ++i)
    {
        size_t key_length = std::min(static_cast<size_t>(8), keys[i].length());
        size_t value_length = values[i].length();

        // Ensure payload does not exceed maximum size
        if (offset + key_length + 2 + value_length > sizeof(packet.payload))
        {
            std::cerr << "Payload size exceeds maximum allowed size" << std::endl;
            return false;
        }

        // Copy key
        std::memcpy(packet.payload + offset, keys[i].c_str(), key_length);
        packet.payload[offset + key_length] = '\0';
        offset += 8;

        // Copy value length (1 byte)
        packet.payload[offset++] = static_cast<uint8_t>(value_length);

        // Copy data type (1 byte)
        packet.payload[offset++] = data_types[i];

        // Copy value
        std::memcpy(packet.payload + offset, values[i].c_str(), value_length);
        offset += value_length;
    }

    packet.payload_length = offset; // Set actual payload length
    packet.flags = 0;
    packet.checksum = calculate_checksum(packet);

    // Send the packet
    return send_packet(sock, dest_addr, packet);
}

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
            std::cout << "Received valid packet from " << src_ip << ":" << ntohs(recvAddr.sin_port) << std::endl;
            std::string topic(recvPacket.topic);
            std::string value(recvPacket.payload, recvPacket.payload_length);

            // Handle payload
            size_t offset = 0;
            std::stringstream result;
            while (offset < recvPacket.payload_length)
            {
                // Extract key (first 8 bytes)
                char key[8] = {0};
                std::memcpy(key, recvPacket.payload + offset, 8);

                offset += 8;

                // Extract value length (next byte)
                uint8_t value_length = recvPacket.payload[offset++];

                // Extract data type (next byte)
                uint8_t data_type = recvPacket.payload[offset++];

                // Extract value (value_length bytes)
                std::string value(reinterpret_cast<char *>(recvPacket.payload + offset), value_length);
                offset += value_length;
                result << key << "-" << value;

                // Add comma if more pairs follow
                if (offset < recvPacket.payload_length)
                {
                    result << ", ";
                }

                std::cout << "Payload details:" << std::endl;
                std::cout << "Key: " << key << std::endl;
                std::cout << "Value: " << value << std::endl;
                std::cout << "Data type: " << static_cast<int>(data_type) << std::endl;
            }
            update_topic(topic, result.str());
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
                ack_cv.notify_one();
            }

            // Update expected sequence number
            expected_seq_num = recvPacket.seq_num + 1;
        }
        else
        {
            std::cerr << "Checksum error!" << std::endl;
        }
    }
}
