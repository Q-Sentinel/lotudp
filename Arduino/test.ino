
#include <WiFi.h>
#include <WiFiUdp.h>

const char *ssid = "Wokwi-GUEST";
const char *password = "";

const char *udpAddress = "192.168.8.141"; // Destination IP address
const int udpPort = 54000;                // Destination UDP port

WiFiUDP udp;

// Define the packet structure
struct Packet
{
    uint8_t version;
    uint8_t type;
    uint32_t seq_num;
    uint16_t payload_length;
    uint8_t flags;
    uint16_t checksum;
    char topic[32];
    char payload[1024]; // Example payload size
};

// Function to calculate a simple checksum
uint16_t calculate_checksum(const Packet &packet)
{
    uint16_t sum = 0;
    sum += packet.version;
    sum += packet.type;
    sum += packet.seq_num & 0xFFFF;
    sum += (packet.seq_num >> 16) & 0xFFFF;
    sum += packet.payload_length;
    sum += packet.flags;
    for (size_t i = 0; i < packet.payload_length; ++i)
    {
        sum += packet.payload[i];
    }
    return ~sum;
}

bool create_and_send_packet(uint32_t &seq_num,
                            const std::vector<String> &keys,
                            const std::vector<String> &values,
                            const std::vector<uint8_t> &data_types, const String &topic)
{
    if (keys.size() != values.size() || keys.size() != data_types.size())
    {
        Serial.println("Mismatch in size of keys, values, or data types");
        return false;
    }

    // Create a packet to send
    Packet packet;
    packet.version = 1;
    packet.type = 1; // Type 1 for data packet
    packet.seq_num = seq_num++;
    packet.flags = 0;

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
            Serial.println("Payload size exceeds maximum allowed size");
            return false;
        }

        // Copy key
        memcpy(packet.payload + offset, keys[i].c_str(), key_length);
        packet.payload[offset + key_length] = '\0';
        offset += 8;

        // Copy value length (1 byte)
        packet.payload[offset++] = static_cast<uint8_t>(value_length);

        // Copy data type (1 byte)
        packet.payload[offset++] = data_types[i];

        // Copy value
        memcpy(packet.payload + offset, values[i].c_str(), value_length);
        offset += value_length;
    }

    packet.payload_length = offset; // Set actual payload length
    packet.checksum = calculate_checksum(packet);

    // Send the packet
    udp.beginPacket(udpAddress, udpPort);
    udp.write((uint8_t *)&packet, sizeof(packet));
    udp.endPacket();

    Serial.println("Packet sent with payload: " + String(packet.payload));
    return true;
}

void setup()
{
    Serial.begin(115200);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }

    Serial.println("Connected to WiFi");

    udp.begin(54001); // Local UDP port for receiving (if needed)
}

void loop()
{
    static uint32_t seq_num = 0;

    std::vector<String> keys = {"key1", "key2"};
    std::vector<String> values = {"value1", "value2"};
    std::vector<uint8_t> data_types = {1, 2}; // Example data types
    String topic = "topic2";

    if (!create_and_send_packet(seq_num, keys, values, data_types, topic))
    {
        Serial.println("Error creating or sending packet, retrying...");
    }

    delay(10000); // Pause before the next iteration
}
