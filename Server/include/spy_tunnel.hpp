#ifndef SPY_TUNNEL_HPP
#define SPY_TUNNEL_HPP

#include "include_tunnel.hpp"

class SpyTunnel {
public:
    int run();
    ~SpyTunnel() = default;
    void shutdown() {
    }
    int init(std::string* __p_ip, 
        uint __port,
        ClientHandler* __p_client,
        std::atomic<bool>* __p_shutdown_flag,
        int __connection_type
    );
protected:
    void write_error(int fifo, const std::string& __msg);
    
    virtual void spawn_window();
    virtual void handle_tunnel(int __socket, int __fifo_fd);
    virtual const char* get_fifo_path();

    std::string* p_ip_;
    uint port_;
    ClientHandler* p_client_;
    std::atomic<bool>* p_shutdown_flag_;
    TunnelFDs* p_tunnel_fds_;
    struct sockaddr_in server_addr_;
    int connection_type_;
    std::thread tunnel_thread_;
};

#endif // SPY_TUNNEL_HPP