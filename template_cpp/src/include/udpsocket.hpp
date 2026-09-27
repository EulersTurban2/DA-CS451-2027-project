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
        if(socketFd >= 0){
            close(socketFd);
        }
    }

    void send(const sockaddr_in& receiver, const uint8_t* message, size_t length){
        sendto(socketFd, message, length, 0, reinterpret_cast<const struct sockaddr*>(&receiver), sizeof(receiver));
    }

    void bind(const sockaddr_in& address){
        if (::bind(socketFd, reinterpret_cast<const struct sockaddr*>(&address), sizeof(address)) < 0) {
            throw std::runtime_error("Failed to bind UDP socket");
        }
    }

    void closeSocket(){
        close(socketFd);
        socketFd = -1;
    }

    ssize_t receive(sockaddr_in& sender, uint8_t* buffer, size_t bufferSize){
        socklen_t senderLen = sizeof(sender);
        ssize_t receivedBytes = recvfrom(socketFd, buffer, bufferSize, 0, reinterpret_cast<struct sockaddr*>(&sender), &senderLen);
        if (receivedBytes < 0) {
            return -1; // Error occurred
        }
        return receivedBytes;
    }

private:

    int socketFd;

};