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

    resize_rgb_data();

    return 0;
}

void CLScreenHunter::run() {
    int rc;
    fd_set fps_fds;

    FD_ZERO(&fps_fds);
    FD_SET(tunnel_socket_, &fps_fds);
    
    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    uint32_t fps_net;
    rc = select(tunnel_socket_ + 1, &fps_fds, nullptr, nullptr, &timeout);
    if (rc <= 0) { // Error or Timeout
        printf("[CLScreenHunter] Error when selecting or timeout received\n");
        return;
    }
    rc = recv(tunnel_socket_, &fps_net, sizeof(fps_net), 0);
    if (rc <= 0) {
        printf("[CLScreenHunter] Failed to receive the fps\n");
        return;
    }
    fps_net = ntohl(fps_net);

    memcpy(&fps_, &fps_net, rc);
    sleep_usec = static_cast<long>((1.0 / fps_) * 1e6);
    printf("Received fps: %.2f -- Time to sleep: %ld μs (%.2f sec)\n", fps_, sleep_usec, sleep_usec / 1e6);

    rc = x11_init();
    if (rc < 0) {
        return;
    }

    printf("[CLScreenHunter] initialization successful\n");

    while (!tunnel_shutdown_flag_) {
        update_gwa();
        if (send_res() < 0) {
            printf("Failed to send res\n");
            break;
        }

        if (get_rgb_data() < 0) {
            printf("Failed get rgb data\n");
            break;
        }

        if (send_rgb_data() < 0) {
            printf("Failed to send rgb data\n");
            break;
        }

        usleep(sleep_usec);
    }    
    x11_cleanup();
}

void CLScreenHunter::update_gwa() {
    XWindowAttributes temp_gwa;
    XGetWindowAttributes(display_, root_, &temp_gwa);
    if (temp_gwa.width != width_ || temp_gwa.height != height_) {
        printf("[CLScreenHunter] Resolution is changed\n");
        gwa_ = temp_gwa;
        width_ = temp_gwa.width;
        height_ = temp_gwa.height;
        resize_rgb_data();
    }
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

    printf("Sent %ld\n", sizeof(res_data));

    return 0;
}

void CLScreenHunter::resize_rgb_data() {
    rgb_data_size_ = (width_ * height_ * 3) + END_KEY_LEN;
    rgb_data_ = (unsigned char*)realloc(rgb_data_, rgb_data_size_);
}

int CLScreenHunter::send_rgb_data() {
    memcpy(rgb_data_ + (rgb_data_size_ - END_KEY_LEN), END_KEY, END_KEY_LEN);

    if (send(tunnel_socket_, rgb_data_, rgb_data_size_, 0) <= 0) {
        printf("[CLScreenHunter] Error occured while sending rgb data\n");
        return -1;
    }

    printf("Sent %ld\n", rgb_data_size_);

    return 0;
}

int CLScreenHunter::get_rgb_data() {
    image_ = XGetImage(display_, root_, 0, 0, width_, height_, AllPlanes, ZPixmap);
    if (!image_) {
        printf("[CLScreenHunter] failed to get image\n");
        return -1;
    }
    for (int y=0;y<image_->height;y++) {
        for (int x=0;x<image_->width;x++) {
            unsigned long pixel = XGetPixel(image_, x, y);
            unsigned char b = (pixel & 0xFF);
            unsigned char g = (pixel & 0xFF00) >> 8;
            unsigned char r = (pixel & 0xFF0000) >> 16;
            int i = (y * image_->width + x) * 3;
            
            rgb_data_[i + 0] = r;
            rgb_data_[i + 1] = g;
            rgb_data_[i + 2] = b;
        }
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

    if (rgb_data_ != nullptr) {
        free(rgb_data_);
    }
}