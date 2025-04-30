#ifndef CLSPYTUNNEL_HPP
#define CLSPYTUNNEL_HPP

#include "cltunnel_include.hpp"

class CLSpyTunnel {
public:
    int init(char* __ip, const uint __port, bool* __p_shutdown_flag);
    void run();    
    void shutdown();
protected:    
    virtual int get_conn_type();
    virtual void exec_tunnel(); // Tunnel specific
    int port_;
    char* ip_;
    int socket_;
    struct sockaddr_in server_addr_;
    bool* p_shutdown_flag_;
    bool tunnel_shutdown_flag_ = false;
};  

#endif // CLSPYTUNNEL_HPP