#ifndef TOPIC_MANAGER_H
#define TOPIC_MANAGER_H
#include "server.h"
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <list>
#include <string>
#include <mutex>
// extern std::unordered_map<std::string, std::list<std::string>> topic_table;
// extern std::mutex table_mutex;
void update_topic(const std::string &topic, const std::string &value);
void clear_topics();
void print_topics();

#endif // TOPIC_MANAGER_H
