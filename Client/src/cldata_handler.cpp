#include "cldata_handler.hpp"
#include "debug.hpp"

int send_out(int __fd, Status __status, sockaddr* __p_tunnel_addr, socklen_t* __p_tunnel_addr_len) {
    char buf[OUT_SIZE];
    
    memcpy(buf, OUT_KEY, OUT_KEY_LEN);
    
    // Little endian format
    buf[OUT_KEY_LEN] = static_cast<char>(__status & 0XFF);
    buf[OUT_KEY_LEN + 1] = static_cast<char>((__status >> 8) & 0XFF);
    
    int rc = 0;
    if (__p_tunnel_addr == nullptr) {
        rc = send(__fd, buf, OUT_SIZE, 0);
    } else {
        rc = sendto(__fd, buf, OUT_SIZE, 0, __p_tunnel_addr, *__p_tunnel_addr_len);
    }

    if (rc < 0) {
        DEBUG_PRINT("[send_out] Error sending data: %s\n", strerror(errno));
        return -1;
    } else if (__p_tunnel_addr == nullptr && rc == 0) {
        DEBUG_PRINT("[send_out] Server closed connection\n");
        return 0;
    } else {
        DEBUG_PRINT("[send_out] Sent %d bytes to server\n", rc);
    }

    return false;
}