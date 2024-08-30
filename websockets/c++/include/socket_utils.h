#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include "packet.h"
#include "topic_manager.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <condition_variable>
#include <queue>
#include <mutex>
#include <time.h>
const int TIMEOUT_MS = 10000;
extern std::queue<uint32_t> ack_queue;
extern std::mutex ack_mutex;
extern std::condition_variable ack_cv;
bool send_packet(int sock, const sockaddr_in &dest_addr, Packet &packet);
bool create_and_send_packet(int sock, const sockaddr_in &dest_addr, uint32_t &seq_num,
                            const std::vector<std::string> &keys,
                            const std::vector<std::string> &values,
                            const std::vector<uint8_t> &data_types, const std::string &topic);
void receive_packets(int sock, uint32_t &expected_seq_num);

#endif // SOCKET_UTILS_H
