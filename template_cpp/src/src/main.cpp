#include <chrono>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <vector>
#include <unordered_map>
#include <thread>

#include "parser.hpp"
#include "udpsocket.hpp"
#include "plink.hpp"
#include "hello.h"
#include <signal.h>


static void buildMapFromIdToAddress(const std::vector<Parser::Host>& hosts, std::unordered_map<unsigned long, sockaddr_in>& processIdToAddress) {
    for (const auto& host : hosts) {
        sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = host.ip;
        address.sin_port = host.port;
        processIdToAddress[host.id] = address;
    }
}

static void parseConfigFile(const std::string& configPath, int& noMsg, unsigned long& receiverId){
  std::ifstream configFile(configPath);
  if (!configFile.is_open()) {
    std::cerr << "Error opening config file: " << configPath << std::endl;
    exit(EXIT_FAILURE);
  }
  configFile >> noMsg >> receiverId;
  configFile.close();
}

static void stop(int) {
  // reset signal handlers to default
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);

  // immediately stop network packet processing
  std::cout << "Immediately stopping network packet processing.\n";

  // write/flush output file if necessary
  std::cout << "Writing output.\n";

  // exit directly from signal handler
  exit(0);
}

int main(int argc, char **argv) {
  signal(SIGTERM, stop);
  signal(SIGINT, stop);

  // `true` means that a config file is required.
  // Call with `false` if no config file is necessary.
  bool requireConfig = true;

  Parser parser(argc, argv);
  parser.parse();

  hello();
  std::cout << std::endl;

  std::cout << "My PID: " << getpid() << "\n";
  std::cout << "From a new terminal type `kill -SIGINT " << getpid() << "` or `kill -SIGTERM "
            << getpid() << "` to stop processing packets\n\n";

  std::cout << "My ID: " << parser.id() << "\n\n";

  std::cout << "List of resolved hosts is:\n";
  std::cout << "==========================\n";
  auto hosts = parser.hosts();
  for (auto &host : hosts) {
    std::cout << host.id << "\n";
    std::cout << "Human-readable IP: " << host.ipReadable() << "\n";
    std::cout << "Machine-readable IP: " << host.ip << "\n";
    std::cout << "Human-readbale Port: " << host.portReadable() << "\n";
    std::cout << "Machine-readbale Port: " << host.port << "\n";
    std::cout << "\n";
  }
  std::cout << "\n";

  std::cout << "Path to output:\n";
  std::cout << "===============\n";
  std::cout << parser.outputPath() << "\n\n";

  std::cout << "Path to config:\n";
  std::cout << "===============\n";
  std::cout << parser.configPath() << "\n\n";
  
  int noMsg = 0;
  unsigned long receiverId = 0;
  parseConfigFile(parser.configPath(), noMsg, receiverId);

  std::cout << "Doing some initialization...\n\n";

  UDPSocket udpSocket;

  std::unordered_map<unsigned long, sockaddr_in> processIdToAddress;
  buildMapFromIdToAddress(hosts, processIdToAddress);
  
  udpSocket.bind(processIdToAddress.at(parser.id()));
  
  PerfectLink perfectLink(udpSocket, [](unsigned long senderId, int seqNr) {
    std::cout << "Delivered message from sender ID: " << senderId << ", sequence number: " << seqNr << "\n";
  }, processIdToAddress, parser.id());

  perfectLink.start();

  if(parser.id() != receiverId){
    for(int i = 1; i <= noMsg; ++i){
      std::string message = "Message " + std::to_string(i) + " from process " + std::to_string(parser.id());
      perfectLink.send(receiverId, i, message);
    }
  }
  
  std::cout << "Broadcasting and delivering messages...\n\n";

  // After a process finishes broadcasting,
  // it waits forever for the delivery of messages.




  while (true) {
    std::this_thread::sleep_for(std::chrono::hours(1));
  }

  return 0;
}
