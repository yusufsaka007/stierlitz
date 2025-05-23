#include "alsa_harvester.hpp"

void ALSAHarvester::exec_spy() {
    int rc = udp_handshake((void*) hw_in_.c_str(), hw_in_.size());
    if (rc < 0) {
        return;
    }

    char status[sizeof(Status) + sizeof(int)];
    Status status_code;
    int argv;

    rc = recvfrom(tunnel_end_socket_, status, sizeof(status), 0, (struct sockaddr*) &tunnel_end_addr_, &tunnel_end_addr_len_);
}

void ALSAHarvester::spawn_window() {
    execlp(
        "urxvt", "urxvt", "-hold", "-name", "stierlitz_alsa_harvester", 
        "-e", ALSA_HARVESTER_SCRIPT_PATH, 
        fifo_path_.c_str(),
        hw_out_.c_str(),
        out_name_.c_str(),
        (char*)NULL
    );
}

void ALSAHarvester::set_hws(const std::string& __hw_in, const std::string& __hw_out) {
    hw_in_ = __hw_in;
    hw_out_ = __hw_out;
}