#include "server.h"

unordered_map<string, vector<websocketpp::connection_hdl>> subscriptions;
mutex subscriptions_mutex;
ws_server *global_server = nullptr; // Initialize global server instance

// Function to notify subscribers
void notify_subscribers(const string &topic, const json &message)
{
    cout << "Sending message to subscribers of topic: " << topic << endl;
    lock_guard<mutex> lock(subscriptions_mutex);
    auto it = subscriptions.find(topic);
    if (it != subscriptions.end())
    {
        for (auto &hdl : it->second)
        {
            cout << "Sending to subscriber" << endl;
            // Send the message to each subscriber
            global_server->send(hdl, message.dump(), websocketpp::frame::opcode::text);
        }
    }
}

// WebSocket Server Message Handler
void on_message(ws_server *server, websocketpp::connection_hdl hdl, ws_server::message_ptr msg)
{
    string payload = msg->get_payload();
    json received_json = json::parse(payload);

    // Process received JSON
    if (received_json.contains("subscribe"))
    {
        string topic = received_json["subscribe"];
        lock_guard<mutex> lock(subscriptions_mutex);
        subscriptions[topic].push_back(hdl);
        cout << "Subscribed to topic: " << topic << endl;
    }
    else if (received_json.contains("update"))
    {
        string topic = received_json["update"];
        json update_message;
        update_message["topic"] = topic;
        update_message["message"] = "Topic updated";
        notify_subscribers(topic, update_message);
    }
}
