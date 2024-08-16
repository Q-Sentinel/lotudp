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
#include <unordered_map>
#include <list>
#include <string>
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
    char topic[32];

    char payload[1024]; // Example payload size
};
std::unordered_map<std::string, std::list<std::string>> topic_table; // Hash table for topics
std::mutex table_mutex;
void print_topics()
{
    std::lock_guard<std::mutex> lock(table_mutex);
    for (const auto &pair : topic_table)
    {
        std::cout << "Topic: " << pair.first << std::endl;
        std::cout << "Values: ";
        for (const auto &value : pair.second)
        {
            std::cout << value << " ";
        }
        std::cout << std::endl;
    }
}
// Function to update the linked list for a given topic
void update_topic(const std::string &topic, const std::string &value)
{
    std::lock_guard<std::mutex> lock(table_mutex);
    auto &list = topic_table[topic]; // Get the list for the topic (create if not exists)
    list.push_front(value);          // Insert the new value at the head
}

// Function to clear the linked list for each topic
void clear_topics()
{
    std::lock_guard<std::mutex> lock(table_mutex);
    for (auto &pair : topic_table)
    {
        pair.second.clear(); // Clear each list
    }
}
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

// Shared resources for acknowledgment handling
std::queue<uint32_t> ack_queue;
std::mutex ack_mutex;
std::condition_variable ack_cv;

// Function to send a packet and wait for acknowledgment
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
            std::cout << "Received valid packet from " << src_ip << ":" << ntohs(recvAddr.sin_port) << std::endl;
            std::string topic(recvPacket.topic);
            std::string value(recvPacket.payload, recvPacket.payload_length);
            update_topic(topic, value);
            // Handle payload
            size_t offset = 0;
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

                std::cout << "Payload details:" << std::endl;
                std::cout << "Key: " << key << std::endl;
                std::cout << "Value: " << value << std::endl;
                std::cout << "Data type: " << static_cast<int>(data_type) << std::endl;
            }

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
void periodic_clear()
{
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::hours(1));
        clear_topics();
        std::cout << "Cleared all topics' linked lists" << std::endl;
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
    addr.sin_port = htons(54001);
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
    sendAddr.sin_port = htons(54000); // Use a different port for sending
    inet_pton(AF_INET, "127.0.0.1", &sendAddr.sin_addr);

    // Start the thread for receiving packets
    uint32_t expected_seq_num = 0;
    std::thread recv_thread(receive_packets, sock, std::ref(expected_seq_num));
    std::thread clear_thread(periodic_clear);
    // Sequence number for outgoing packets
    uint32_t seq_num = 0;

    // Loop to send packets
    while (true)
    {
        std::vector<std::string> keys = {"key1", "key2"};
        std::vector<std::string> values = {"value1", "value2"};
        std::vector<uint8_t> data_types = {1, 2}; // Example data types
        std::string topic = "topic1";
        // Create and send the packet
        if (!create_and_send_packet(sock, sendAddr, seq_num, keys, values, data_types, topic))
        {
            std::cerr << "Error creating or sending packet, retrying..." << std::endl;
            continue; // Retry sending the packet
        }
        print_topics();
        // Pause before the next iteration
        std::this_thread::sleep_for(std::chrono::seconds(SEND_DELAY_S));
    }

    // Clean up and close the socket
    close(sock);
    recv_thread.join(); // Wait for the receive thread to finish
    return 0;
}
