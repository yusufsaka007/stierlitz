#ifndef SCREEN_HUNTER_HPP
#define SCREEN_HUNTER_HPP

#include "include_tunnel.hpp"
#include "spy_tunnel.hpp"

class ScreenHunter : public SpyTunnel {
public:
    ScreenHunter() = default;
    float fps_ = 0.1;
    long timeout_ms_;
    void set_fps(float __fps);
protected:
    void spawn_window() override;
    void exec_spy() override;
};

#endif // SCREEN_HUNTER_HPP