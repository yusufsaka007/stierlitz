#include "clspy_tunnel.hpp"

void* cltunnel_helper(void* arg) {
    CLTunnel* tunnel = static_cast<CLTunnel*>(arg);
    tunnel->clspy_tunnel_->run();

    printf("WTFFFFFFFFF\n");

    // Cleanup
    delete tunnel->clspy_tunnel_;
    tunnel->clspy_tunnel_ = nullptr;
    tunnel->thread_running_ = false;
    return nullptr;
}

int CLSpyTunnel::init(const char* __ip, const int __port, const int __connection_type) {
    memset(argv_, 0, BUFFER_SIZE);
    memset(&tunnel_addr_, 0, sizeof(tunnel_addr_));
    tunnel_addr_.sin_family = AF_INET;
    tunnel_addr_.sin_port = htons(__port);
    if (inet_pton(AF_INET, __ip, &tunnel_addr_.sin_addr) <= 0) {
        printf("[CLSpyTunnel] Invalid address or address not supported: %s\n", __ip);
        return -1;
    }
    tunnel_addr_len_ = sizeof(tunnel_addr_);

    int tries = 0;

    while (tries < 5) {
        tunnel_socket_ = socket(AF_INET, __connection_type, 0);
        if (tunnel_socket_ < 0) {
            printf("[CLSpyTunnel] Socket creation failed: %s\n", strerror(errno));
            return -1;
        }

        if (__connection_type == UDP_BASED) {
            return 0;
        }

        if (connect(tunnel_socket_, (struct sockaddr*)&tunnel_addr_, tunnel_addr_len_) < 0) {
            close(tunnel_socket_);
            if (errno == ECONNREFUSED || errno == ETIMEDOUT) {
                printf("[CLSpyTunnel] Trying to connect to the tunnel server\n");
                sleep(1); // Retry after a short delay
                continue;
            } else {
                printf("[CLSpyTunnel] Connection failed: %s\n", strerror(errno));
                return -1;
            }
        }

        printf("[CLSpyTunnel] Connected to tunnel server\n");
        break;
    }
    if (tries == 5) {
        printf("[CLSpyTunnel] Failed to connect to tunnel server after multiple attempts\n");
        return -1;
    }
    return 0;
}

int CLSpyTunnel::udp_handshake() {
    Status handshake = HEY_ATTACKER_PLEASE_DOMINATE_ME;
    sendto(tunnel_socket_, &handshake, sizeof(Status), 0, (struct sockaddr*) &tunnel_addr_, tunnel_addr_len_);

    handshake = UDP_ACK;
    while (true) {
        int rc = recvfrom(tunnel_socket_, argv_, BUFFER_SIZE, 0, (struct sockaddr*) &tunnel_addr_, &tunnel_addr_len_);
        printf("[udp_handshake] Received: %d\n", *reinterpret_cast<uint32_t*>(argv_));
        if (rc <= 0) {
            printf("[udp_handshake] Error while receiving the argument from server\n");
            return -1;
        }
        rc = sendto(tunnel_socket_, &handshake, sizeof(Status), 0, (struct sockaddr*) &tunnel_addr_, tunnel_addr_len_);
        if (rc <= 0) {
            printf("[udp_handshake] Error while sending ACK\n");
            return -1;
        }

        return 0;
    }
}

void CLSpyTunnel::run() {
    // Implement the tunnel logic here
    while (!tunnel_shutdown_flag_.load()) {
        const char* test = "test";
        send(tunnel_socket_, test, strlen(test), 0);
        sleep(1);

    }
}