/**
 * @file Server/server.cpp
 * @author viv4ldi
 * @brief Handles the server and clients
 * @version 0.1
 */
#include "server.hpp"

Server::Server() {
    port_ = DEFAULT_PORT;
    ip_ = DEFAULT_IP;
    max_connections_ = DEFAULT_MAX_CONNECTIONS;
}
Server::Server(const std::string& __ip, const uint32_t __port, const uint8_t __max_connections) {
    port_ = __port;
    ip_ = __ip;
    max_connections_ = __max_connections;
    server_socket_ = -1;
}

int Server::init() {
    int rc;
    
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ == -1) {
        std::cerr << "[Server::init] Error creating socket" << std::endl;
        return -1;
    }

    int opt=1;
    rc = setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (rc < 0) {
        std::cerr << "[Server::init] Error setting socket options" << std::endl;
        return -1;
    }

    server_addr_.sin_family = AF_INET;
    server_addr_.sin_port = htons(port_);
    rc = inet_pton(AF_INET, ip_.c_str(), &server_addr_.sin_addr);
    if (rc == 0) {
        std::cerr << RED << "[Server::init] IP address is not valid" << RESET << std::endl;
        return -1;
    }
    else if (rc < 0) {
        std::cerr << RED << "[Server::init] Error converting IP address: " << strerror(errno) << RESET << std::endl; // Use strerror_r for thread safety in the clients
        return -1;
    }

    rc = bind(server_socket_, (const struct sockaddr*) &server_addr_, sizeof(server_addr_));
    if (rc < 0) {
        std::cerr << RED << "[Server::init] Error binding socket: " << strerror(errno) << RESET << std::endl;
        return -1;
    }

    rc = listen(server_socket_, max_connections_);
    if (rc < 0) {
        std::cerr << RED << "[Server::init] Error listening on socket: " << strerror(errno) << RESET << std::endl;
        return -1;
    }

    return 0;
}

void Server::start() {
    int rc = 0;
    clients_ = new ClientHandler*[max_connections_];
    for (int i = 0; i < max_connections_; i++) {
        clients_[i] = new ClientHandler();
    }

    while (!shutdown_flag_) {
        
    }
}

void Server::shutdown() { 
    shutdown_flag_ = true;
    for (auto& thread : threads_) {
        if (thread->joinable()) {
            thread->join();
        }
    }

    for (int i = 0; i < max_connections_; i++) {
        if (clients_[i] != nullptr) {
            clients_[i]->cleanup_client();
            delete clients_[i];
            clients_[i] = nullptr;
        }
    }
}