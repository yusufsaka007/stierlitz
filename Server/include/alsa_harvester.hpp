#ifndef ALSA_HARVESTER_HPP
#define ALSA_HARVESTER_HPP

#include "spy_tunnel.hpp"

class ALSAHarvester : public SpyTunnel {
public:
    ALSAHarvester() = default;
    void set_hws(const std::string& __hw_in, const std::string& __hw_out);
private:
    void exec_spy() override;
    void spawn_window() override;    

    std::string hw_in_;
    std::string hw_out_;
};

#endif // ALSA_HARVESTER_HPP