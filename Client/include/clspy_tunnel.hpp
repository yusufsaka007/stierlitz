#ifndef CLSPY_TUNNEL_CPP
#define CLSPY_TUNNEL_CPP

#include "cltunnel_include.hpp"

class CLSpyTunnel {
public:
    CLSpyTunnel() = default;
    int init(const char* __ip, const int __port, const int __connection_type);
    virtual void run();
    std::atomic<bool> tunnel_shutdown_flag_ = false;
protected:
    int tunnel_socket_;
    struct sockaddr_in tunnel_addr_;
};

struct CLTunnel {
    CLSpyTunnel* clspy_tunnel_ = nullptr;
    pthread_t thread;
    bool thread_running_ = false;
};

void* cltunnel_helper(void* arg);

#endif // CLSPY_TUNNEL_CPP