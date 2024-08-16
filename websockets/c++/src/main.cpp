#include "server.h"
#include "packet.h"
#include "topic_manager.h"
#include "socket_utils.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace std;
using namespace websocketpp;
using json = nlohmann::json;

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
// WebSocket Server Initialization
int main()
{
    // WebSocket server setup
    ws_server server;
    global_server = &server;

    try
    {
        server.set_message_handler(bind(&on_message, &server, std::placeholders::_1, std::placeholders::_2));

        server.init_asio();
        server.listen(9002);
        server.start_accept();

        std::cout << "WebSocket server running on port 9002" << std::endl;

        // Run the WebSocket server in a separate thread
        std::thread server_thread([&server]()
                                  { server.run(); });

        // UDP socket setup
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0)
        {
            std::cerr << "Error creating socket" << std::endl;
            return 1;
        }

        // Set up the address for receiving packets
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(54000);
        // for local host
        // inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        // for all
        inet_pton(AF_INET, "0.0.0.0", &addr.sin_addr);
        if (::bind(sock, (sockaddr *)&addr, sizeof(addr)) < 0)
        {
            std::cerr << "Error binding socket" << std::endl;
            ::close(sock);
            return 1;
        }

        // Set up the address for sending messages
        sockaddr_in sendAddr;
        sendAddr.sin_family = AF_INET;
        sendAddr.sin_port = htons(54001);
        inet_pton(AF_INET, "127.0.0.1", &sendAddr.sin_addr);

        // Start the UDP receiving thread
        uint32_t expected_seq_num = 0;
        std::thread recv_thread(receive_packets, sock, std::ref(expected_seq_num));

        // Start the periodic clearing thread
        std::thread clear_thread(periodic_clear);

        // Sequence number for outgoing packets
        uint32_t seq_num = 0;

        // Main loop to send packets
        // while (true)
        // {
        //     std::vector<std::string> keys = {"key1", "key2"};
        //     std::vector<std::string> values = {"value1", "value2"};
        //     std::vector<uint8_t> data_types = {1, 2};
        //     std::string topic = "topic1";

        //     // Create and send the packet (you should implement this function)
        //     if (!create_and_send_packet(sock, sendAddr, seq_num, keys, values, data_types, topic))
        //     {
        //         std::cerr << "Error creating or sending packet, retrying..." << std::endl;
        //         continue;
        //     }

        //     print_topics(); // Implement your topic printing logic here

        //     // Notify all subscribers after a short delay
        //     std::this_thread::sleep_for(std::chrono::seconds(10));
        //     json test_message;
        //     test_message["message"] = "Hello, subscribers!";
        //     notify_subscribers("topic1", test_message);

        //     std::this_thread::sleep_for(std::chrono::seconds()));
        // }

        // Cleanup
        server_thread.join();
        recv_thread.join();
        clear_thread.join();
        ::close(sock);
    }
    catch (const websocketpp::exception &e)
    {
        std::cerr << "WebSocket++ exception: " << e.what() << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Standard exception: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "Unknown exception caught" << std::endl;
    }

    return 0;
}
