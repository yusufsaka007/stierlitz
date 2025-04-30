#ifndef SCOPED_EPOLL_HPP
#define SCOPED_EPOLL_HPP

#include <unistd.h>

struct ScopedEpollFD {
    int fd = -1;
    ScopedEpollFD();
    ~ScopedEpollFD();
};


#endif // SCOPED_EPOLL_HPP