#ifndef CLKEYLOGGER_HPP
#define CLKEYLOGGER_HPP

#include "cltunnel_include.hpp"
#include "clspytunnel.hpp"

class CLKeylogger : public CLSpyTunnel {
public:
    int get_conn_type() override;
};

#endif // CLKEYLOGGER_HPP