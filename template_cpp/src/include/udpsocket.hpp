#pragma once

#include <cstdint>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

class UDPSocket {

public:
    UDPSocket() {
        socketFd = socket(AF_INET, SOCK_DGRAM, 0);
        if (socketFd < 0) {
            throw std::runtime_error("Failed to create UDP socket");
        }
    }
    ~UDPSocket() {
        close(socketFd);
    }

    void send(const sockaddr_in& receiver, const uint8_t* message, size_t length){
        sendto(socketFd, message, length, 0, (const struct sockaddr*)&receiver, sizeof(receiver));
    }

    void bind(const sockaddr_in& address){
        if (::bind(socketFd, (const struct sockaddr*)&address, sizeof(address)) < 0) {
            throw std::runtime_error("Failed to bind UDP socket");
        }
    }

    int receive(sockaddr_in& sender, uint8_t* buffer, size_t bufferSize){
        socklen_t senderLen = sizeof(sender);
        ssize_t receivedBytes = recvfrom(socketFd, buffer, bufferSize, 0, (struct sockaddr*)&sender, &senderLen);
        if (receivedBytes < 0) {
            throw std::runtime_error("Failed to receive UDP message");
        }
        return receivedBytes;
    }

private:

    int socketFd;

};