#include "data_handler.hpp"

#ifdef SERVER
int recv_buf(int __fd, char* __buf, int __len, const std::atomic<bool>& __shutdown_flag) {
    int rc;
}
#endif // SERVER