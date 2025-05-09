#ifndef CLPACKET_TUNNEL_HPP
#define CLPACKET_TUNNEL_HPP

#include "cltunnel_include.hpp"
#include "clspy_tunnel.hpp"

class CLPacketTunnel : public CLSpyTunnel {
public:
    void run() override;
};

#endif // CLPACKET_TUNNEL_HPP