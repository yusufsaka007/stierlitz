#ifndef SPY_TUNNEL_HPP
#define SPY_TUNNEL_HPP

#include "include_tunnel.hpp"

class SpyTunnel {
public:
    SpyTunnel(
        std::string* __p_ip, 
        uint* __p_port,
        int __client_index,
        std::atomic<bool>* __p_shutdown_flag,
        int __connection_type,
        std::shared_ptr<LogContext> __log_context,
        int __shutdown_event_fd
    );

    int run();
    ~SpyTunnel() = default;
    void shutdown() {
    }
private:
    void write_error(int fifo, const std::string& __msg);
protected:
    virtual void spawn_window();
    virtual void handle_tunnel(int __socket, int __fifo_fd);
    virtual const char* get_fifo_path();

    std::string* p_ip_;
    uint* p_port_;
    int client_index_;
    std::atomic<bool>* p_shutdown_flag_;
    int socket_;
    struct sockaddr_in server_addr_;
    int connection_type_;
    std::thread tunnel_thread_;
    std::shared_ptr<LogContext> log_context_;
    int shutdown_event_fd_;
    EventLog event_log_;
};

#endif // SPY_TUNNEL_HPP