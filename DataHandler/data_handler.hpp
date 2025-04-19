#ifndef DATA_HANDLER_HPP
#define DATA_HANDLER_HPP

#include <iostream>
#include <atomic>
#include <sys/socket.h>
#include <string>
#include <cstring>
#include <sys/epoll.h>
#include <unistd.h>

#define PEER_DISCONNECTED_ERROR 0
#define EPOLL_CREATE_ERROR -1
#define EPOLL_CTL_ERROR -2
#define EPOLL_WAIT_ERROR -3
#define RECV_ERROR -4
#define SEND_ERROR -5

struct ScopedEpollFD {
    int fd = -1;
    ScopedEpollFD();
    ~ScopedEpollFD();
};

#ifdef SERVER
/**
 * @brief Checks if the shutdown flag is set in a non-blocking environment for an appropriate shutdown
 */
int recv_buf(int __fd, char* __buf, int __len, const std::atomic<bool>& __shutdown_flag);
int send_buf(int __fd, const char* __buf, int __len, const std::atomic<bool>& __shutdown_flag);
#endif // SERVER

#endif // DATA_HANDLER_HPP