#pragma once

#include <arpa/inet.h>
#include <atomic>
#include <cstring>
#include <netinet/in.h>
#include <string>
#include <mutex>
#include <functional>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <iostream>

#include "udpsocket.hpp"
#include "utils.hpp"


#define DEBUG 1

class PerfectLink{

public:
    PerfectLink(UDPSocket& socket, std::function<void(unsigned long senderId, int seqNr)> callback, 
    const std::unordered_map<unsigned long, sockaddr_in>& processIdToAddress, unsigned long processId) : 
            udpSocket(socket), deliverCallback(callback), processIdToAddress(processIdToAddress), processId(processId) {}
    ~PerfectLink() {
        stop();
    }

    void send(unsigned long receiverId, int seqNr, const std::string& message){
        /* TODO */
        // First part we create the message buffer with the format: [ACK flag(1 byte)][senderId(8 bytes)][seqNr(4 bytes)][message]
        std::vector<uint8_t> buffer;
        buffer.resize(HEADER_SIZE + message.size());
        buffer[0] = 0; // ACK flag
        *reinterpret_cast<unsigned long*>(buffer.data() + 1) = processId;
        *reinterpret_cast<int*>(buffer.data() + 1 + sizeof(unsigned long)) = seqNr;
        memcpy(buffer.data() + HEADER_SIZE, message.data(), message.size());
        struct sockaddr_in receiver = processIdToAddress.at(receiverId);
        udpSocket.send(receiver, buffer.data(), buffer.size());
        std::lock_guard<std::mutex> lock(mtx);
        pendingMessages[std::make_pair(receiverId, seqNr)] = std::make_pair(receiver, std::vector<uint8_t>(buffer));
        #if DEBUG
        std::cout << "Sent message to receiver ID: " << receiverId << ", sequence number: " << seqNr << ", message: " << message << "\n";
        #endif 
    }

    void retransmissionLoop(){
        while(!stopRetransmission.load()){
            {
                std::lock_guard<std::mutex> lock(mtx);
                for(auto& entry : pendingMessages){
                    const auto& receiver = entry.second.first;
                    const auto& buffer = entry.second.second;
                    udpSocket.send(receiver, buffer.data(), buffer.size());
                    #if DEBUG
                    std::cout << "Retransmitted message to receiver ID: " << entry.first.first << ", sequence number: " << entry.first.second << "\n";
                    #endif 
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void stopRetransmissionLoop(){
        stopRetransmission.store(true);
    }


    void receive(){
        sockaddr_in sender{};
        uint8_t buffer[2048]{};
        size_t bufferSize = sizeof(buffer);
        while(!stopRetransmission.load()){
            ssize_t receivedBytes = udpSocket.receive(sender, buffer, bufferSize);
            #if DEBUG 
            std::cout << "Received " << receivedBytes << " bytes from " << inet_ntoa(sender.sin_addr) << ":" << ntohs(sender.sin_port) << "\n";
            #endif
            if(receivedBytes > 0){
                if(buffer[0] == 1){
                    //ACK
                    unsigned long senderId = *reinterpret_cast<unsigned long*>(buffer + 1);
                    int seqNr = *reinterpret_cast<int*>(buffer + 1 + sizeof(unsigned long));
                    std::lock_guard<std::mutex> lock(mtx);
                    pendingMessages.erase(std::make_pair(senderId, seqNr));
                    #if DEBUG
                    std::cout << "Received ACK for message from sender ID: " << senderId << ", sequence number: " << seqNr << "\n";
                    #endif
                }
                else{
                    //Message
                    unsigned long senderId = *reinterpret_cast<unsigned long*>(buffer + 1);
                    int seqNr = *reinterpret_cast<int*>(buffer + 1 + sizeof(unsigned long));
                    std::string message(reinterpret_cast<char*>(buffer + HEADER_SIZE), receivedBytes - HEADER_SIZE);

                    std::lock_guard<std::mutex> lock(mtx);
                    auto deliveredKey = std::make_pair(senderId, seqNr);
                    if(deliveredMessages.find(deliveredKey) == deliveredMessages.end()){
                        // Deliver the message
                        deliverCallback(senderId, seqNr);
                        deliveredMessages.insert(deliveredKey);
                    }

                    #if DEBUG
                    std::cout << "Delivered message from sender ID: " << senderId << ", sequence number: " << seqNr << ", message: " << message << "\n";
                    #endif

                    uint8_t ackBuffer[HEADER_SIZE];
                    ackBuffer[0] = 1; // ACK flag
                    *reinterpret_cast<unsigned long*>(ackBuffer + 1) = processId;
                    *reinterpret_cast<int*>(ackBuffer + 1 + sizeof(unsigned long)) = seqNr;
                    udpSocket.send(sender, ackBuffer, sizeof(ackBuffer));
                }
            } else if(receivedBytes < 0){
                continue; // Error occurred, continue to the next iteration
            }
        }
    }


    void start(){
        receiveThread = std::thread(&PerfectLink::receive, this);
        retransmissionThread = std::thread(&PerfectLink::retransmissionLoop, this);
    }

    void stop(){
        stopRetransmissionLoop();
        if(receiveThread.joinable()){
            udpSocket.closeSocket(); // Close the socket to unblock the receive call
            receiveThread.join();
        }
        if(retransmissionThread.joinable()){
            retransmissionThread.join();
        }
    }

private:
    static constexpr size_t HEADER_SIZE = 1 + sizeof(unsigned long) + sizeof(int);

    UDPSocket& udpSocket;
    std::function<void(unsigned long senderId, int seqNr)> deliverCallback;
    std::unordered_map<unsigned long, sockaddr_in> processIdToAddress;
    unsigned long processId;


    std::thread receiveThread;
    std::thread retransmissionThread;

    std::unordered_set<std::pair<unsigned long, int>, Utils::hashPair> deliveredMessages;
    std::unordered_map<std::pair<unsigned long, int>, std::pair<sockaddr_in, std::vector<uint8_t>>, Utils::hashPair> pendingMessages;

    std::mutex mtx;
    std::atomic<bool> stopRetransmission{false};
};