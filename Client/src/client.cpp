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
    uint8_t command_byte;
    char buffer[MAX_BUFFER_SIZE];
    int bytes_received = 0;
    int bytes_sent = 0;
    int error = 0;
    socklen_t len = sizeof(error);

    while (!shutdown_flag_) {
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
            bytes_received = recv(socket_, &command_byte, 1, 0);
            if (bytes_received < 0) {
                printf("Error receiving data\n");
                break;
            } else if (bytes_received == 0) {
                printf("Server closed connection\n");
                break;
            }
            switch (command_byte) {
                case TEST:
                    printf("Received TEST command\n");
                    strncpy(buffer, OUT_KEY, OUT_KEY_SIZE);
                    strncat(buffer, "Test command received\n", sizeof(buffer) - OUT_KEY_SIZE - 1);
                    buffer[sizeof(buffer) - 1] = '\0'; 
                    bytes_sent = send(socket_, buffer, strlen(buffer), 0);
                    if (bytes_sent < 0) {
                        printf("Error sending data\n");
                    } else {
                        printf("Sent TEST response to server\n");
                    }
                    break;
                case KILL:
                    printf("Received KILL command\n");
                    shutdown_flag_ = true;
                    break;
                default:
                    break;
            }
        }
        retry:
            sleep(1);
            if (socket_ >= 0) {
                close(socket_);
            }
            continue;
    }
}

void Client::shutdown() {
    printf("Shutting down client\n");
    close(socket_);
}