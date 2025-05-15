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

}

bool CLScreenHunter::is_res_changed() {
    XRRScreenConfiguration* conf = XRRGetScreenInfo(display_, root_);
    if (!conf) {
        XCloseDisplay(display_);
        display_ = nullptr;
        return false;
    }

    Rotation rot;
    SizeID size_id = XRRConfigCurrentConfiguration(conf, &rot);
    XRRScreenSize* sizes;
    int num_sizes;
    sizes = XRRSizes(display_, 0, &num_sizes);

    bool changed = false;
    if (sizes && size_id < num_sizes) {
        int new_width = sizes[size_id].width;
        int new_height = sizes[size_id].height;

        if (new_width != width_ || new_height != height_) {
            changed = true;
        }
    }

    XRRFreeScreenConfigInfo(conf);
    return changed;
}

int CLScreenHunter::update_gwa() {
    XWindowAttributes temp_gwa;
    XGetWindowAttributes(display_, root_, &temp_gwa);
    if (width_ != temp_gwa.width || height_ != temp_gwa.height) {
        gwa_ = std::move(temp_gwa);
        return -1;
    }

    return 0;
}

int CLScreenHunter::send_res() {
}