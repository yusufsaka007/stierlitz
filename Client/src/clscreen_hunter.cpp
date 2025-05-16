#include "clscreen_hunter.hpp"

int CLScreenHunter::x11_init() {
    display_ = XOpenDisplay(nullptr);
    if (!display_) {
        printf("[CLScreenHunter] Can't open display\n");
        return -1;
    }

    root_ = DefaultRootWindow(display_);

    // For initial handshake (width and height is sent)
    // gwa_, width_ and height_ might change over time
    XGetWindowAttributes(display_, root_, &gwa_);

    width_ = gwa_.width;
    height_ = gwa_.height;

    if (send_res() < 0) {
        return -1;
    }

    return 0;
}

void CLScreenHunter::run() {
    int rc = x11_init();
    if (rc < 0) {
        return;
    }

    printf("[CLScreenHunter] initialization successful\n");

    char command[1024];
    fd_set recv_fds;

    while (!tunnel_shutdown_flag_) {
        FD_ZERO(&recv_fds);
        FD_SET(tunnel_socket_, &recv_fds);

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 300000;
        
        int rc = select(tunnel_socket_ + 1, &recv_fds, nullptr, nullptr, &timeout);
        if (rc < 0) {
            printf("[CLScreenHunter] error while selecting\n");
            break;
        } else if (rc == 0) {
            // Timeout 
            continue;
        }
        
        if (FD_ISSET(tunnel_socket_, &recv_fds)) {
            rc = recv(tunnel_socket_, command, sizeof(command), 0);
            if (rc <= 0) {
                printf("[CLScreenHunter] error while receiving\n");
                break;
            }
            if (strncmp(command, "exit", 4) == 0) {
                printf("[CLScreenHunter] exit received\n");
                break;
            } else if (strncmp(command, "ss", 2) == 0) {
                printf("[CLScreenHunter] ss received\n");
                if (update_gwa() != 0) {
                    // Resolution is changed, send the new one
                    send_res();
                }
                if (send_rgb_data() < 0) {
                    break;
                }
            } else {
                printf("[CLScreenHunter] Wtf is received%s\n", command);
            }
        }
    }
    
    x11_cleanup();
}

int CLScreenHunter::update_gwa() {
    XWindowAttributes temp_gwa;
    XGetWindowAttributes(display_, root_, &temp_gwa);
    if (temp_gwa.width != width_ || temp_gwa.height != height_) {
        printf("[CLScreenHunter] Resolution is changed\n");
        gwa_ = temp_gwa;
        width_ = temp_gwa.width;
        height_ = temp_gwa.height;
        return -1;
    }

    return 0;
}

int CLScreenHunter::send_res() {
    uint8_t res_data[RES_UPDATE_KEY_LEN + 8];
    memcpy(res_data, &width_, sizeof(width_));
    memcpy(res_data + 4, &height_, sizeof(height_));
    memcpy(res_data + 8, RES_UPDATE_KEY, RES_UPDATE_KEY_LEN);

    if (send(tunnel_socket_, res_data, sizeof(res_data), 0) <= 0) {
        printf("[CLScreenHunter] Error occured while sending resolution\n");
        return -1;
    }

    return 0;
}

int CLScreenHunter::send_rgb_data() {
    memset(rgb_data_, 0, 16);
    memcpy(rgb_data_ + 16, END_KEY, END_KEY_LEN);

    if (send(tunnel_socket_, rgb_data_, 16+END_KEY_LEN, 0) <= 0) {
        printf("[CLScreenHunter] Error occured while sending rgb data\n");
        return -1;
    }

    return 0;
}

void CLScreenHunter::x11_cleanup() {
    if (image_ != nullptr) {
        XDestroyImage(image_);
    }

    if (display_ != nullptr) {
        XCloseDisplay(display_);
    }
}