#ifndef SPY_TUNNEL_HPP
#define SPY_TUNNEL_HPP

#include <iostream>
#include <string>
#include <atomic>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

class SpyTunnel {
public:
    SpyTunnel(
        std::string* __p_ip, 
        int* __p_port, 
        std::atomic<bool>* __p_shutdown_flag,
        int __connection_type
    );

    int init();
    virtual ~SpyTunnel();
    virtual void run();
protected:
    std::string* p_ip_;
    int* p_port_;
    std::atomic<bool>* p_shutdown_flag_;
    int socket_;
    int connection_type_;
};

#endif // SPY_TUNNEL_HPP