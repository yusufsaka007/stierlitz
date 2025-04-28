/**
 * @file Server/client.cpp
 * @author viv4ldi
 * @brief Class implementation for the client handler
 * @version 0.1
 */

#include "client_handler.hpp"

ClientHandler::ClientHandler() {
    addr_len_ = sizeof(addr_);
    set_values();
    for (int i = 0; i < TUNNEL_NUMS; i++) {
        tunnel_types_[i] = -1;
        tunnel_fds_[i] = -1;
        tunnel_shutdown_fds_[i] = -1;
    }
}

int ClientHandler::is_client_up(const int __fd) {
    char buf;
    int rc = recv(__fd, &buf, 1, MSG_PEEK | MSG_DONTWAIT);

    if (rc < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        return -1;
    }
    return 0;
}

void ClientHandler::init_client(sockaddr_in __client_addr, int __client_socket, int __index) {
    addr_ = __client_addr;
    socket_ = __client_socket;
    index_ = __index;
    ip_ = inet_ntoa(addr_.sin_addr);
}

int ClientHandler::socket() const {
    return socket_;
}

std::string ClientHandler::ip() const {
    return ip_; 
}

int ClientHandler::index() const {
    return index_;
}

void ClientHandler::set_values() {
    socket_ = -1;
    ip_ = "";
    index_ = -1;
    memset(&addr_, 0, addr_len_);
}

void ClientHandler::cleanup_client() {
    if (ClientHandler::is_client_up(socket_) == 0) {
        close(socket_);
    }

    set_values();
}

int* ClientHandler::set_tunnel(uint8_t __tunnel) {
    if (active_tunnels_ & __tunnel) {
        return nullptr; // Tunnel already set
    }
    active_tunnels_ |= __tunnel;
    for (int i=0;i<TUNNEL_NUMS;i++) {
        if (tunnel_types_[i] == -1) {
            tunnel_types_[i] = __tunnel;
            return &tunnel_fds_[i];
        }
    }
    return nullptr; // No free tunnel slot available
}

int ClientHandler::unset_tunnel(uint8_t __tunnel) {
    if (!(active_tunnels_ & __tunnel)) {
        return -1; // Tunnel not set 
    }
    active_tunnels_ &= ~__tunnel;
    for (int i=0;i<TUNNEL_NUMS;i++) {
        if (tunnel_types_[i] == __tunnel) {
            tunnel_types_[i] = -1;
            
            return tunnel_fds_[i];
        }
    }
    return -1; // Tunnel not found
}

TunnelFDs ClientHandler::operator[](uint8_t __tunnel) const {
    for (int i=0;i<TUNNEL_NUMS;i++) {
        if (tunnel_types_[i] == __tunnel) {
            return {tunnel_shutdown_fds_[i], tunnel_fds_[i]};
        }
    }
    return {-1, -1}; // Tunnel not found
}