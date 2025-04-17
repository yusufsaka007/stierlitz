/**
 * @file Server/client.cpp
 * @author viv4ldi
 * @brief Class implementation for the client handler
 * @version 0.1
 */

#include "client_handler.hpp"

ClientHandler::ClientHandler() {
    addr_len_ = sizeof(client_addr_);
    set_values();
}

int ClientHandler::get_client_socket() const {
    return client_socket_;
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

void ClientHandler::init_client(sockaddr_in_ __client_addr, int __client_socket, int __index) {
    client_addr_ = __client_addr;
    client_socket_ = __client_socket;
    index_ = __index;
    ip_ = inet_ntoa(client_addr_.sin_addr);
}

void ClientHandler::set_values() {
    client_socket_ = -1;
    ip_ = "";
    index_ = -1;
    bzero(client_addr_, addr_len_);
}

void ClientHandler::cleanup_client() {
    if (ClientHandler::is_client_up(client_socket_) == 0) {
        close(client_socket_);
    }

    set_values();
}