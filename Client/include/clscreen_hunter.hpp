#ifndef CLSCREEN_HUNTER_HPP
#define CLSCREEN_HUNTER_HPP

#include "cltunnel_include.hpp"
#include "clspy_tunnel.hpp"
#include <X11/Xlib.h>
#include <X11/Xutil.h>

class CLScreenHunter : public CLSpyTunnel {
public:
    void run() override;
protected:
    uint32_t width_;
    uint32_t height_;
    Display* display_ = nullptr;
    Window root_;
    XWindowAttributes gwa_;
    XImage* image_ = nullptr;
    unsigned char rgb_data_[BUFFER_SIZE];

    int x11_init();
    int update_gwa();
    int get_rgb_data();
    int send_res();
    int send_rgb_data();
    void x11_cleanup();
};

#endif // CLSCREEN_HUNTER_HPP