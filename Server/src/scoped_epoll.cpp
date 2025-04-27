#include "scoped_epoll.h"

ScopedEpollFD::ScopedEpollFD(){
    fd = -1;
}
ScopedEpollFD::~ScopedEpollFD() {
    if (fd >= 0) {
        close(fd);
    }
}
