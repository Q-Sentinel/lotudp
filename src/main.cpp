#include "packet.h"
#include "socket_utils.h"
#include "topic_manager.h"
#include <thread>
#include <iostream>

std::queue<uint32_t> ack_queue;
std::mutex ack_mutex;
std::condition_variable ack_cv;
const int SEND_DELAY_S = 20;
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
