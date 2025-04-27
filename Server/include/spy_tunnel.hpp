#ifndef SPY_TUNNEL_HPP
#define SPY_TUNNEL_HPP

#include <iostream>
#include <string>
#include <atomic>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <signal.h>
#include <sys/wait.h>
#include "server_macros.hpp"

class SpyTunnel {
public:
    SpyTunnel(
        std::string* __p_ip, 
        uint* __p_port,
        int __client_index,
        std::atomic<bool>* __p_shutdown_flag,
        int __connection_type
    );

    int run();
    ~SpyTunnel() = default;
    void shutdown() {
    }
protected:
    virtual void spawn_window();
    virtual void handle_tunnel(int __write_pipe);

    std::string* p_ip_;
    uint* p_port_;
    int client_index_;
    std::atomic<bool>* p_shutdown_flag_;
    int socket_;
    struct sockaddr_in server_addr_;
    int connection_type_;
    std::thread tunnel_thread_;
};

#endif // SPY_TUNNEL_HPP