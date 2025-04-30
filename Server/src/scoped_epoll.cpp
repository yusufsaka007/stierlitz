#include "scoped_epoll.hpp"

ScopedEpollFD::ScopedEpollFD(){
    fd = -1;
}
ScopedEpollFD::~ScopedEpollFD() {
    if (fd >= 0) {
        close(fd);
    }
}
