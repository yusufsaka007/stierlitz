#include "cldata_handler.hpp"

bool send_out(int __fd, Status __status) {
    char buf[OUT_SIZE];
    
    memcpy(buf, OUT_KEY, OUT_KEY_LEN);
    
    // Little endian format
    buf[OUT_KEY_LEN] = static_cast<char>(__status & 0XFF);
    buf[OUT_KEY_LEN + 1] = static_cast<char>((__status >> 8) & 0XFF);
    
    int rc = send(__fd, buf, OUT_SIZE, 0);
    if (rc < 0) {
        printf("[send_out] Error sending data: %s\n", strerror(errno));
        return true;
    } else if (rc == 0) {
        printf("[send_out] Server closed connection\n");
        return true;
    } else {
        printf("[send_out] Sent %d bytes to server\n", rc);
    }

    return false;
}