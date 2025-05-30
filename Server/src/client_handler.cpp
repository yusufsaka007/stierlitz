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
}

int ClientHandler::is_client_up(const int __fd) {
    if (__fd < 0) {
        return -1;
    }
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