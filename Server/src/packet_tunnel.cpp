#include "packet_tunnel.hpp"

PacketTunnel::PacketTunnel(const std::string& __file_name, const std::string& __out_name, EventLog* __p_event_log, size_t __limit) 
: file_name_(__file_name), p_event_log_(__p_event_log), limit_(__limit) {
    out_name_ = __out_name;
}

void PacketTunnel::run() { // No need for spawning a window. Just receive the file
    tunnel_shutdown_fd_.fd = eventfd(0, EFD_NONBLOCK);
    if (tunnel_shutdown_fd_.fd == -1) {
        write_fifo_error("[SpyTunnel::run] Error creating shutdown event file descriptor " + std::string(strerror(errno)));
        return;
    }


    int rc = accept_tunnel_end();
    if (rc < 0) {
        return;
    }

    // Send the file name first
    rc = send(tunnel_end_socket_, file_name_.c_str(), file_name_.size(), 0);
    if (rc < 0) {
        *p_event_log_ << LOG_MUST << RED << "[PacketTunnel::run] Error while sending the file name: " << strerror(errno) << RESET_C2_FIFO;
        return;
    } else if (rc == 0) {
        *p_event_log_ << LOG_MUST << RED << "[PacketTunnel::run] Peer disconnected while sending file name"<< RESET_C2_FIFO;
        return;
    }

    // Recv the file 
    // **WARNING** this saves the file contents inside the memory until it is done
    // Receiving large files might cause program to act sluggish
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(tunnel_end_socket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    ScopedBuf* buf = new ScopedBuf();
    rc = recv_all(tunnel_end_socket_, buf, limit_);
    if (rc <= 0) {
        *p_event_log_ << LOG_MUST << RED << "[PacketTunnel::run] Error while receiving: " << rc << RESET_C2_FIFO;
        delete buf;
        return;
    }

    size_t received = get_total_size(buf);
    *p_event_log_ << LOG_MUST << GREEN << "[PacketTunnel::run] Received with size: " << received << RESET_C2_FIFO;

    // Save the file
    std::ofstream out(out_name_, std::ios::binary);
    if (!out.is_open()) {
        *p_event_log_ << LOG_MUST << RED << "[PacketTunnel::run] Failed to open output file: " << out_name_ << RESET_C2_FIFO;
        delete buf;
        return;
    }

    for (ScopedBuf* current = buf; current != nullptr;current=current->next_) {
        if (current->buf_ && current->len_ > 0) {
            out.write(current->buf_, current->len_);
        }
    }

    out.close();
    *p_event_log_ << LOG_MUST << GREEN << "[PacketTunnel::run] File saved to: " << out_name_ << RESET_C2_FIFO;

    delete buf;
}