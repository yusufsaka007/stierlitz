#ifndef SCREEN_HUNTER_HPP
#define SCREEN_HUNTER_HPP

#include "include_tunnel.hpp"
#include "spy_tunnel.hpp"

class ScreenHunter : public SpyTunnel {
public:
    ScreenHunter() = default;
protected:
    void spawn_window() override;
    void exec_spy() override;
    void close_fifos();

    std::string fifo_in_;
    std::string fifo_data_;

    int tunnel_fifo_in_ = -1;
    int tunnel_fifo_data_ = -1;
};

#endif // SCREEN_HUNTER_HPP