#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <string>
#include <thread>
#include <signal.h>
#include <memory>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <fcntl.h>
#include <vector>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include "event_logger.hpp"
#include "client_handler.hpp"
#include "server_macros.hpp"
#include "data_handler.hpp"

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
    void handle_client(ClientHandler* client);
    void handle_command();
    void log_event();
    void log(std::stringstream& __event_log);
private: // Variables
    uint32_t port_;
    std::string ip_;
    uint8_t max_connections_;
    int user_verbosity_ = 0;

    std::atomic<bool> shutdown_flag_ = false;;

    struct sockaddr_in server_addr_;
    int server_socket_;
    ScopedEpollFD shutdown_event_fd_;

    ClientHandler** clients_;
    int client_count_ = 0;
    std::vector<std::unique_ptr<std::thread>> threads_;
    std::mutex accept_mutex_;
    std::mutex client_mutex_;
    std::condition_variable accept_cv_;

    std::mutex log_mutex_;
    std::condition_variable log_cv_;
    std::queue<std::string> log_queue_;
};

#endif // SERVER_HPP