#ifndef CLIENT_HANDLER_HPP
#define CLIENT_HANDLER_HPP

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <spy_tunnel.hpp>
#include "common.hpp"

struct TunnelFDs {
    int tunnel_fd_;
    int shutdown_fd_;
};

class ClientHandler{
public:
    ClientHandler();
    static int is_client_up(const int __fd);
    void cleanup_client();
    void init_client(sockaddr_in __client_addr, int __client_socket, int __index);
    int socket() const;
    std::string ip() const;
    int index() const;
    int set_tunnel(SpyTunnel* __p_tunnel, uint8_t __tunnel_code);
    int unset_tunnel(uint8_t __tunnel_code);
    TunnelFDs* operator[](uint8_t __tunnel_code);
private:
    void set_values();
    int socket_;
    std::string ip_;
    struct sockaddr_in addr_;
    socklen_t addr_len_;
    int index_;
    uint8_t active_tunnels_ = 0x00;
    uint8_t tunnel_codes_[TUNNEL_NUMS];
    TunnelFDs tunnel_fds_[TUNNEL_NUMS];
    SpyTunnel* tunnels_[TUNNEL_NUMS]; // This pointer is used to access tunnel during cleanup
};

#endif // CLIENT_HANDLER_HPP