#ifndef SERVER_H
#define SERVER_H

#include <iostream>
#include <unordered_map>
#include <string>
#include <mutex>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <nlohmann/json.hpp> // For JSON handling

using namespace std;
using namespace websocketpp;
using json = nlohmann::json;

// Define WebSocket Server type
typedef server<websocketpp::config::asio> ws_server;

// Global variables
extern unordered_map<string, vector<websocketpp::connection_hdl>> subscriptions;
extern mutex subscriptions_mutex;
extern ws_server *global_server; // Global server instance

// Function to notify subscribers
void notify_subscribers(const string &topic, const json &message);

// WebSocket Server Message Handler
void on_message(ws_server *server, websocketpp::connection_hdl hdl, ws_server::message_ptr msg);

#endif // SERVER_H
