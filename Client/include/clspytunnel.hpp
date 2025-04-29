#ifndef CLSPYTUNNEL_HPP
#define CLSPYTUNNEL_HPP

#include "cltunnel_include.hpp"

class CLSpyTunnel {
public:
    int init(char* __ip, const uint __port, int* __p_shutdown_flag);
    void shutdown();
    virtual int get_conn_type();
protected:
    int port_;
    char* ip_;
    int socket_;
    struct sockaddr_in server_addr_;
    int* p_shutdown_flag_;
    bool retry_flag_ = false;
};  

#endif // CLSPYTUNNEL_HPP