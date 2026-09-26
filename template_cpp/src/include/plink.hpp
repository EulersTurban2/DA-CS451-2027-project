#pragma once

#include <atomic>
#include <string>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <unordered_set>

#include "udpsocket.hpp"
#include "utils.hpp"

class PerfectLink{

public:
    PerfectLink(UDPSocket& socket, std::function<void(int senderId, int seqNr)> callback, int processId) : udpSocket(socket), deliverCallback(callback), processId(processId) {}
    ~PerfectLink() {}

    void send(int receiverId, int seqNr, const std::string& message){
        /* TODO */
        // retransmission loop logic with a timeout
        
    }

    void receive(){
        sockaddr_in sender{};
        uint8_t buffer[2048]{};
        size_t bufferSize = sizeof(buffer);
        while(!stopRetransmission.load()){
            int receivedBytes = udpSocket.receive(sender, buffer, bufferSize);
            if(receivedBytes > 0){
                if(buffer[0] == 1){
                    //ACK
                    int senderId = *reinterpret_cast<int*>(buffer + 1);
                    int seqNr = *reinterpret_cast<int*>(buffer + 1 + sizeof(int));
                    std::lock_guard<std::mutex> lock(mtx);
                    pendingMessages.erase(std::make_pair(senderId, seqNr));
                }
                else{
                    //Message
                    int senderId = *reinterpret_cast<int*>(buffer + 1);
                    int seqNr = *reinterpret_cast<int*>(buffer + 1 + sizeof(int));
                    std::string message(reinterpret_cast<char*>(buffer + 1 + 2 * sizeof(int)), receivedBytes - 1 - 2 * sizeof(int));

                    std::lock_guard<std::mutex> lock(mtx);
                    auto deliveredKey = std::make_pair(senderId, seqNr);
                    if(deliveredMessages.find(deliveredKey) == deliveredMessages.end()){
                        // Deliver the message
                        deliverCallback(senderId, seqNr);
                        deliveredMessages.insert(deliveredKey);
                    }

                    uint8_t ackBuffer[1 + 2 * sizeof(int)];
                    ackBuffer[0] = 1; // ACK flag
                    *reinterpret_cast<int*>(ackBuffer + 1) = processId;
                    *reinterpret_cast<int*>(ackBuffer + 1 + sizeof(int)) = seqNr;
                    udpSocket.send(sender, ackBuffer, sizeof(ackBuffer));
                }
            }
        }

    }
private:

    int processId;

    UDPSocket& udpSocket;
    std::function<void(int senderId, int seqNr)> deliverCallback;

    std::unordered_set<std::pair<int, int>, Utils::hashPair> deliveredMessages;
    std::unordered_map<std::pair<int,int>, std::pair<sockaddr_in, std::vector<uint8_t>>, Utils::hashPair> pendingMessages;

    std::mutex mtx;
    std::atomic<bool> stopRetransmission{false};
};