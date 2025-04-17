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

    memset(&server_addr_, 0, sizeof(server_addr_));
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

    threads_.resize(max_connections_);

    return 0;
}

void Server::start() {
    int rc = 0;
    clients_ = new ClientHandler*[max_connections_];
    for (int i = 0; i < max_connections_; i++) {
        clients_[i] = nullptr;
    }

    /**
     * Sarts the threads for 
     * accepting (which will start the client threads upon connection)
     * command executor
     * logger
     */

     std::thread accept_thread(&Server::accept_client, this);
}

int Server::accept_client() {
    int rc;
    int client_socket;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    bool slot_found = false;

    while (!shutdown_flag_) {
        // Wait for a free slot in clients_ or shutdown signal
        std::unique_lock<std::mutex> lock(accept_mutex_);
        accept_cv_.wait(lock, [this] {return client_count_ < max_connections_ || shutdown_flag_;});
        if (shutdown_flag_) {
            std::cout << YELLOW << "[Server::accept_client] Shutdown signal received" << RESET << std::endl;
            return 0;
        }

        ClientHandler* client = nullptr;

        client_socket = accept(server_socket_, (struct sockaddr*) &client_addr, &addr_len);
        if (client_socket < 0) {
            if (errno == EINTR) {
                std::cerr << YELLOW << "[Server::accept_client] Shutdown signal received" << RESET << std::endl;
                return 0;
            } else {
                std::cerr << RED << "[Server::accept_client] Error accepting client: " << strerror(errno) << RESET << std::endl;
                return -1;
            }
        } else {
            std::cout << GREEN << "[Server::accept_client] Client connected" << RESET << std::endl;
            
            {
                // Search for a free slot in clients_ for the new client
                std::lock_guard<std::mutex> lock(client_mutex_);
                for (int i = 0; i < max_connections_; i++) {
                    if (clients_[i] == nullptr) {
                        client = new ClientHandler();
                        if (client == nullptr) {
                            std::cerr << RED << "[Server::accept_client] Error allocating memory for client" << RESET << std::endl;
                            close(client_socket);
                            return -1;
                        }

                        client->init_client(client_addr, client_socket, i);
                        clients_[i] = client;
                        client_count_++;
                        slot_found = true;

                        std::cout << GREEN << "[Server::accept_client] Client connected:" << client->ip() << RESET << std::endl;
                        break;
                    }
                }
            }

            if (!slot_found){
                std::cerr << RED << "[Server::accept_client] No free slots for new client" << RESET << std::endl;
                close(client_socket);
                return -1;
            }
            
            // Start a new handle_client thread for the client
            threads_[client->index()] = std::make_unique<std::thread>(&Server::handle_client, this, client);
            if (threads_[client->index()] == nullptr) {
                std::cerr << RED << "[Server::accept_client] Error creating thread for client" << RESET << std::endl;
                {
                    std::lock_guard<std::mutex> lock(client_mutex_);
                    client->cleanup_client();
                    delete client;
                    clients_[client->index()] = nullptr;
                    client_count_--;
                }
                return -1;
            }
        }
    }
}

void Server::handle_client(ClientHandler* client) {
    
}

void Server::handle_command() {

}

void Server::console_logger() {

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