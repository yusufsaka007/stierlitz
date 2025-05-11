#ifndef SPY_TUNNEL_HPP
#define SPY_TUNNEL_HPP

#include "include_tunnel.hpp"

class SpyTunnel {
public:
    SpyTunnel() = default;
    virtual ~SpyTunnel();
    int init(const std::string& __ip, uint __port, int*& p_tunnel_shutdown_fd, int __connection_type);
    virtual void run();
    
    void shutdown();
    void edit_fifo_path(int __client_index, CommandCode __command_code);
    void set_dev(int __dev_num);
    void set_out(const std::string& __out_name);
protected:
    uint32_t dev_num_;
    std::string out_name_{""};
    struct sockaddr_in tunnel_addr_;
    int tunnel_socket_ = -1;
    int tunnel_end_socket_ = -1;
    ScopedEpollFD tunnel_shutdown_fd_;
    int tunnel_fifo_ = -1;
    pid_t pid_ = -1;
    std::string fifo_path_;
    struct sockaddr_in tunnel_end_addr_;
    socklen_t tunnel_end_addr_len_;

    virtual void exec_spy();
    int accept_tunnel_end();
    int create_epoll_fd(ScopedEpollFD& __epoll_fd);
    void write_fifo_error(const std::string& __msg);
    int udp_handshake(void* __arg, int __len);
    virtual void spawn_window();
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