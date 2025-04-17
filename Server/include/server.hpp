#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <signal.h>
#include <memory>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <vector>
#include "client_handler.hpp"
#include "macros.hpp"

class Server {
public:
    Server();
    Server(const std::string& __ip, const uint32_t __port, const uint8_t __max_connections);
    ~Server();
    int init();
    void start();
    void shutdown();
private: // Functions
    int accept_client();
private: // Variables
    uint32_t port_;
    std::string ip_;
    uint8_t max_connections_;
    struct sockaddr_in server_addr_;
    int server_socket_;
    std::atomic<bool> shutdown_flag_ = false;;
    ClientHandler** clients_;
    std::vector<std::unique_ptr<std::thread>> threads_;
    std::mutex client_mutex_;
    std::condition_variable client_cv_;

};

#endif // SERVER_HPP