#ifndef DATA_HANDLER_HPP
#define DATA_HANDLER_HPP

#include <iostream>
#include <atomic>
#include <sys/socket.h>
#include <string>
#include <cstring>
#include <sys/epoll.h>
#include <unistd.h>
#include <common.hpp>
#include "scoped_epoll.hpp"

#define PEER_DISCONNECTED_ERROR 0
#define EPOLL_CREATE_ERROR -1
#define EPOLL_CTL_ERROR -2
#define EPOLL_WAIT_ERROR -3
#define RECV_ERROR -4
#define SEND_ERROR -5
#define LIMIT_PASSED_ERROR -6
#define TIMEOUT_ERROR -7
#define MEMORY_ERROR -8

#define DEFAULT_SIZE_LIMIT 10240;


struct ScopedBuf {
    char* buf_;
    size_t len_ = 0;
    ScopedBuf* next_ = nullptr;
    ~ScopedBuf();
};

int recv_all(int __fd, ScopedBuf* __buf, size_t size_limit=0);
size_t get_total_size(ScopedBuf* __buf);
int recv_buf(int __fd, char* __buf, int __len, const std::atomic<bool>& __shutdown_flag);
int send_buf(int __fd, const char* __buf, int __len, const std::atomic<bool>& __shutdown_flag);

#endif // DATA_HANDLER_HPP