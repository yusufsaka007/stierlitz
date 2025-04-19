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

Server::~Server() {
    if (!shutdown_flag_) {
        shutdown();
    }
}

int Server::init() {
    int rc;
    
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ == -1) {
        std::cerr << "[Server::init] Error creating socket" << std::endl;
        return -1;
    }

    // Set server to non blocking and reusable
    int flags = fcntl(server_socket_, F_GETFL, 0);
    if (flags == -1) {
        std::cerr << "[Server::init] Error getting socket flags" << std::endl;
        return -1;
    }
    rc = fcntl(server_socket_, F_SETFL, flags | O_NONBLOCK);
    if (rc == -1) {
        std::cerr << "[Server::init] Error setting socket to non-blocking" << std::endl;
        return -1;
    }

    int opt = 1;
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

    shutdown_event_fd_.fd = eventfd(0, EFD_NONBLOCK);
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
    std::thread logger_thread(&Server::log_event, this);

    logger_thread.join();
    accept_thread.join();
}

int Server::accept_client() {
    int rc;
    int client_socket;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    bool slot_found;
    std::stringstream event_log;

    ScopedEpollFD epoll_fd;
    epoll_fd.fd = epoll_create1(0);
    if (epoll_fd.fd == -1) {
        std::cerr << RED << "[Server::accept_client] Error creating epoll instance: " << strerror(errno) << RESET << std::endl;
        return -1;
    }

    struct epoll_event ev;
    ev.data.fd = server_socket_;
    ev.events = EPOLLIN;
    if (epoll_ctl(epoll_fd.fd, EPOLL_CTL_ADD, server_socket_, &ev) == -1) {
        std::cerr << RED << "[Server::accept_client] Error adding server socket to epoll: " << strerror(errno) << RESET << std::endl;
        return -1;
    }

    struct epoll_event shutdown_event;
    shutdown_event.data.fd = shutdown_event_fd_.fd;
    shutdown_event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd.fd, EPOLL_CTL_ADD, shutdown_event_fd_.fd, &shutdown_event) == -1) {
        std::cerr << RED << "[Server::accept_client] Error adding shutdown eventfd to epoll: " << strerror(errno) << RESET << std::endl;
        return -1;
    }

    struct epoll_event events[2];

    while (!shutdown_flag_) {
        slot_found = false;

        // Wait for a free slot in clients_ or shutdown signal
        std::unique_lock<std::mutex> lock(accept_mutex_);
        accept_cv_.wait(lock, [this] {return client_count_ < max_connections_ || shutdown_flag_;});
        if (shutdown_flag_) {
            std::cout << YELLOW << "[Server::accept_client] Shutdown signal received" << RESET << std::endl;
            return 0;
        }

        // std::cout << GREEN << "[Server::accept_client] Waiting for client connection" << RESET << std::endl;
        event_log << GREEN << "[Server::accept_client] Waiting for client connection";
        log(event_log);
        
        int nfds = epoll_wait(epoll_fd.fd, events, 2, -1);
        if (nfds == -1) {
            if (errno == EINTR) {
                std::cerr << YELLOW << "[Server::accept_client] Shutdown signal received" << RESET << std::endl;
                return 0;
            } else {
                std::cerr << RED << "[Server::accept_client] Error waiting for epoll event: " << strerror(errno) << RESET << std::endl;
                return -1;
            }
        } 
        
        for (int i=0; i<nfds; i++) {
            if (events[i].data.fd == shutdown_event_fd_.fd) {
                //std::cout << YELLOW << "[Server::accept_client] Shutdown signal received" << RESET << std::endl;
                event_log << YELLOW << "[Server::accept_client] Shutdown signal received";
                log(event_log);

                uint64_t u;
                read(shutdown_event_fd_.fd, &u, sizeof(u));
                return 0;
            }

            if(events[i].data.fd == server_socket_) {
                client_socket = accept(server_socket_, (struct sockaddr*) &client_addr, &addr_len);
                if (client_socket < 0) {   
                    std::cerr << RED << "[Server::accept_client] Error accepting client: " << strerror(errno) << RESET << std::endl;
                    return -1;
                }
                
                ClientHandler* client = nullptr;       
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
                            // std::cout << GREEN << "[Server::accept_client] New connection from: " << client->ip() << RESET << std::endl;
                            
                            event_log << GREEN << "[Server::accept_client] New connection from: " << client->ip();
                            log(event_log);
                            
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
                try {
                    if (threads_[client->index()] != nullptr && threads_[client->index()]->joinable()) {
                        threads_[client->index()]->join();
                    }
                    log(event_log);

                    threads_[client->index()] = std::make_unique<std::thread>(&Server::handle_client, this, client);
                } catch (const std::system_error& e) {
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

    return 0;
}

void Server::handle_client(ClientHandler* client) {
    // Receive data from the client
    char buffer[MAX_BUFFER_SIZE];
    int recv_rc;
    int send_rc;
    std::stringstream event_log;

    while (!shutdown_flag_) {
        memset(buffer, 0, sizeof(buffer));
        recv_rc = recv_buf(client->socket(), buffer, sizeof(buffer), shutdown_flag_);
        if (recv_rc < 0) {
            std::cerr << RED << "[Server::handle_client] Client " << client->index() << " failed: " << recv_rc << RESET << std::endl;  
            break; // @todo find a better way to handle recv error
        } else if (recv_rc == PEER_DISCONNECTED_ERROR) {
            std::cerr << RED << "[Server::handle_client] Client: " << client->index() << " disconnected" << RESET << std::endl;
            break; // @todo find a better way to handle client disconnection
        } else {
            // Test
            //std::cout << GREEN << "[Server::handle_client] Data from client " << client->index() << ": " << buffer << RESET << std::endl;
            event_log <<  GREEN << "[Server::handle_client] Data from client " << client->index() << ": " << buffer;
            log(event_log);

            send_rc = send_buf(client->socket(), buffer, recv_rc, shutdown_flag_);
            if (send_rc < 0) {
                std::cerr << RED << "[Server::handle_client] Client " << client->index() << " failed: " << send_rc << RESET << std::endl;
                break; // @todo find a better way to handle send error
            } else if (send_rc == PEER_DISCONNECTED_ERROR) {
                std::cerr << RED << "[Server::handle_client] Client: " << client->index() << " disconnected" << RESET << std::endl;
                break; // @todo find a better way to handle client disconnection
            }
        }
    }

    std::cout << YELLOW << "[Server::handle_client] Cleaning up client " << client->index() << RESET << std::endl;
    {
        std::lock_guard<std::mutex> lock(client_mutex_);
        clients_[client->index()] = nullptr;
        client->cleanup_client();
        delete client;
        client_count_--;
        accept_cv_.notify_one();
    }
}

void Server::log(std::stringstream& __event_log) {
    {
        std::lock_guard<std::mutex> lock(log_mutex_);
        log_queue_.push(__event_log.str());
        log_cv_.notify_one();
    }
    __event_log.str("");
}

void Server::log_event() {
    while (!shutdown_flag_) {
        std::unique_lock<std::mutex> lock(log_mutex_);
        log_cv_.wait(lock, [this] {return !log_queue_.empty() || shutdown_flag_;});
        while (!log_queue_.empty()) {
            std::string log_message = std::move(log_queue_.front());
            log_queue_.pop();
            std::cout << log_message << RESET << std::endl;
        }

        if (shutdown_flag_) {
            std::cout << YELLOW << "[Server::log_event] Shutdown signal received" << RESET << std::endl;
            break;
        }
    }
}

void Server::handle_command() {

}

void Server::shutdown() { 
    uint64_t u = 1;
    write(shutdown_event_fd_.fd, &u, sizeof(u)); // Notify the accept thread to stop waiting

    shutdown_flag_ = true;
    for (auto& thread : threads_) {
        if (thread && thread->joinable()) {
            thread->join();
        }
    }

    for (int i = 0; i < max_connections_; i++) {
        if (clients_[i] != nullptr) {
            clients_[i]->cleanup_client();
            delete clients_[i];
        }
    }

    delete[] clients_;
    
    if (server_socket_ != -1) {
        close(server_socket_);
    }
}