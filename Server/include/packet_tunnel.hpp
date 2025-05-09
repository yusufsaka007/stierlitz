#ifndef PACKET_TUNNEL_HPP
#define PACKET_TUNNEL_HPP

#include "include_tunnel.hpp"
#include "spy_tunnel.hpp"

class PacketTunnel : public SpyTunnel {
public:
    PacketTunnel(const std::string& __file_name, const std::string& __out_name, EventLog* __p_event_log, size_t __limit=0);
    void run() override;
protected:
    EventLog* p_event_log_;
    std::string file_name_;
    size_t limit_ = 0;
};

#endif // PACKET_TUNNEL_HPP