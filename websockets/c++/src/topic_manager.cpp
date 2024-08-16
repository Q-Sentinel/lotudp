#include "topic_manager.h"
#include <iostream>

std::unordered_map<std::string, std::list<std::string>> topic_table;
std::mutex table_mutex;

void update_topic(const std::string &topic, const std::string &value)
{

    std::lock_guard<std::mutex> lock(table_mutex);
    auto &list = topic_table[topic];
    list.push_front(value);
    json update_message;
    update_message["topic"] = topic;
    update_message["message"] = value;
    notify_subscribers(topic, update_message);
}

void clear_topics()
{
    std::lock_guard<std::mutex> lock(table_mutex);
    for (auto &pair : topic_table)
    {

        pair.second.clear();
    }
}

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
