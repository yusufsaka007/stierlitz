#include "data_handler.hpp"

ScopedEpollFD::ScopedEpollFD(){
    fd = -1;
}
ScopedEpollFD::~ScopedEpollFD() {
    if (fd >= 0) {
        close(fd);
    }
}

#ifdef SERVER
int recv_buf(int __fd, char* __buf, int __len, const std::atomic<bool>& __shutdown_flag) {
    int rc = 0;
    ScopedEpollFD epoll_fd;
    epoll_fd.fd = epoll_create1(0);
    if (epoll_fd.fd == -1) {
        return EPOLL_CREATE_ERROR;
    }

    epoll_event ev, events[1];
    ev.events = EPOLLIN;
    ev.data.fd = __fd;
    if (epoll_ctl(epoll_fd.fd, EPOLL_CTL_ADD, __fd, &ev) == -1) {
        return EPOLL_CTL_ERROR;
    }

    while (!__shutdown_flag.load()) {
        int nfds = epoll_wait(epoll_fd.fd, events, 1, 500);
        if (nfds == -1) {
            rc = EPOLL_WAIT_ERROR;
            break;
        } else if (nfds == 0) {
            continue; // Timeout, no events
        }

        if (events[0].data.fd == __fd) {
            ssize_t bytes_received = recv(__fd, __buf, __len, 0);
            if (bytes_received < 0) {
                rc = RECV_ERROR;
                break;
            } else if (bytes_received == 0) {
                return PEER_DISCONNECTED_ERROR;
            } else {
                for (int i=bytes_received; i>0; i--) {
                    if(__buf[i-1] == '\n' || __buf[i-1] == '\r') {
                        __buf[i-1] = '\0';
                        bytes_received--;
                        break;
                    }
                }
                if (bytes_received < __len) {
                    __buf[bytes_received] = '\0';
                } else {
                    __buf[__len - 1] = '\0';
                }
                __buf[bytes_received] = '\0';
                return bytes_received;
            }
        }
    }
    return rc;

}

int send_buf(int __fd, const char* __buf, int __len, const std::atomic<bool>& __shutdown_flag) {
    ssize_t bytes_sent;
    ssize_t total_bytes = 0;

    if (__len == -1) {
        __len = strlen(__buf);
    }

    while (!__shutdown_flag.load() && total_bytes < __len) {
        bytes_sent = send(__fd, __buf + total_bytes, __len - total_bytes, 0);
        if (bytes_sent < 0) {
            return SEND_ERROR;
        } else if (bytes_sent == 0) {
            return PEER_DISCONNECTED_ERROR; // Connection closed
        }

        total_bytes += bytes_sent;
    }

    return total_bytes;
}

#endif // SERVER