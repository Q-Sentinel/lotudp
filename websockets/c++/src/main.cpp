#include "server.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace std;
using namespace websocketpp;
using json = nlohmann::json;

// WebSocket Server Initialization
int main()
{
    ws_server server;
    global_server = &server; // Set the global server instance

    try
    {
        server.set_message_handler(bind(&on_message, &server, std::placeholders::_1, std::placeholders::_2));

        server.init_asio();
        server.listen(9002);
        server.start_accept();

        cout << "WebSocket server running on port 9002" << endl;

        // Run the server in a separate thread
        std::thread server_thread([&server]()
                                  { server.run(); });

        // Wait for the server to start and accept connections
        std::this_thread::sleep_for(std::chrono::seconds(10));

        // Notify all subscribers
        json test_message;
        test_message["message"] = "Hello, subscribers!";
        notify_subscribers("topic1", test_message);

        // Allow some time to send notifications before shutting down
        std::this_thread::sleep_for(std::chrono::seconds(10));

        // Cleanup
        server_thread.join();
    }
    catch (const websocketpp::exception &e)
    {
        cerr << "WebSocket++ exception: " << e.what() << endl;
    }
    catch (const std::exception &e)
    {
        cerr << "Standard exception: " << e.what() << endl;
    }
    catch (...)
    {
        cerr << "Unknown exception caught" << endl;
    }

    return 0;
}
