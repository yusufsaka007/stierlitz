#ifndef CLSPY_TUNNEL_CPP
#define CLSPY_TUNNEL_CPP

#include "cltunnel_include.hpp"

class CLSpyTunnel {
public:
    CLSpyTunnel() = default;
    virtual ~CLSpyTunnel() = default;
    int init(const char* __ip, const int __port, const int __connection_type);
    int udp_handshake();
    virtual void run();
    std::atomic<bool> tunnel_shutdown_flag_ = false;
protected:
    int tunnel_socket_;
    struct sockaddr_in tunnel_addr_;
    socklen_t tunnel_addr_len_ = 0;
    char argv_[BUFFER_SIZE];
};

struct CLTunnel {
    CLSpyTunnel* clspy_tunnel_ = nullptr;
    pthread_t thread;
    bool thread_running_ = false;
};

void* cltunnel_helper(void* arg);

#endif // CLSPY_TUNNEL_CPP