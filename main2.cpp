#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

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

    // Loop to send and receive messages
    while (true)
    {
        std::cout << ".";
        // Send a message
        const char *msg = "Hello from sender!";
        int sendResult = sendto(sock, msg, strlen(msg), 0, (sockaddr *)&sendAddr, sizeof(sendAddr));
        if (sendResult < 0)
        {
            std::cerr << "Error sending data" << std::endl;
            close(sock);
            return 1;
        }
        std::cout << "Sent: " << msg << std::endl;

        // Receive a message
        char buffer[1024];
        sockaddr_in recvAddr;
        socklen_t recvAddrLen = sizeof(recvAddr);

        int bytesReceived = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr *)&recvAddr, &recvAddrLen);
        if (bytesReceived < 0)
        {
            std::cerr << "Error receiving data" << std::endl;
            continue;
        }

        buffer[bytesReceived] = '\0'; // Null-terminate the received data
        std::cout << "Received: " << buffer << std::endl;

        // Pause before the next iteration
        sleep(1);
    }

    close(sock);
    return 0;
}
