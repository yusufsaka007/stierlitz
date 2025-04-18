#ifndef DATA_HANDLER_HPP
#define DATA_HANDLER_HPP

#include <iostream>
#include <atomic>
#include <sys/socket.h>
#include <string>
#include <cstring>
#include <sys/epoll.h>

enum DataHandlerError {
    PEER_DISCONNECTED_ERROR = 0,
    EPOLL_CREATE_ERROR = -1,
    EPOLL_CTL_ERROR = -2,
    EPOLL_WAIT_ERROR = -3,
    RECV_ERROR = -4,
    SEND_ERROR = -5,
};

int recv_buf(int __fd, char* __buf, int __len);
#ifdef SERVER
/**
 * @brief Checks if the shutdown flag is set in a non-blocking environment for an appropriate shutdown
 */
int recv_buf(int __fd, char* __buf, int __len, const std::atomic<bool>& __shutdown_flag);
int send_buf(int __fd, const char* __buf, int __len, const std::atomic<bool>& __shutdown_flag);
#endif // SERVER

#endif // DATA_HANDLER_HPP