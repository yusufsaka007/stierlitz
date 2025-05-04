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
#include <sys/wait.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include "event_logger.hpp"
#include "client_handler.hpp"
#include "server_macros.hpp"
#include "data_handler.hpp"
#include "command_handler.hpp"

class Server {
public:
    Server();
    Server(const std::string& __ip, const uint __port, const int __max_connections);
    ~Server();
    int init();
    void start();
    void shutdown();
private: // Functions
    int accept_client();
    void handle_client(ClientHandler* client);
    void handle_c2();
    void log_event();
    void cleanup_server();
    void cleanup_tunnel_queue();
private: // Variables
    uint port_;
    std::string ip_;
    uint8_t max_connections_;
    int user_verbosity_;

    std::atomic<bool> shutdown_flag_ = false;;

    struct sockaddr_in server_addr_;
    int server_socket_;
    ScopedEpollFD shutdown_event_fd_;
    C2FIFO c2_fifo_;

    std::vector<ClientHandler*> clients_;
    int client_count_ = 0;
    std::vector<std::thread> client_threads_;
    std::mutex accept_mutex_;
    std::mutex client_mutex_;
    std::condition_variable accept_cv_;
    std::shared_ptr<LogContext> log_context_;

    std::vector<Tunnel*> tunnels_;
    std::shared_ptr<TunnelContext> tunnel_context_;
};

#endif // SERVER_HPP