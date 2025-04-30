#include "clkeylogger.hpp"

int CLKeylogger::get_conn_type() {
    return TCP_BASED;
}

void CLKeylogger::exec_tunnel() {
    const char* buffer = "test";
    while (*p_shutdown_flag_) {
        int rc = send(socket_, buffer, strlen(buffer), 0);
        if (rc < 0) {
            printf("Error sending data: %s\n", strerror(errno));
            break;
        } else if (rc == 0) {
            printf("Server closed connection\n");
            break;
        } else {
            printf("Sent %d bytes to server\n", rc);
        }
        sleep(1);
    }
}