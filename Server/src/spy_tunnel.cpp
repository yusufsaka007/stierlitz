#include "spy_tunnel.hpp"

SpyTunnel::SpyTunnel(std::string* __p_ip, int* __p_port, std::atomic<bool>* __p_shutdown_flag, int __connection_type) {    
    p_ip_ = __p_ip;
    p_port_ = __p_port;
    p_shutdown_flag_ = __p_shutdown_flag;
    socket_ = -1;
    connection_type_ = __connection_type;
}

int SpyTunnel::init() {
    socket_ = socket(AF_INET, connection_type_, 0);
    if (socket_ == -1) {
    }
}