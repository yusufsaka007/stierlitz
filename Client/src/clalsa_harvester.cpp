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
        memcpy(status + sizeof(Status), &rc, sizeof(rc));
        sendto(tunnel_socket_, status, sizeof(status), 0, (struct sockaddr*) &tunnel_addr_, tunnel_addr_len_);
    } else {
        // Send success + channel num
        memcpy(status, &status_code, sizeof(Status));
        memcpy(status + sizeof(Status), &channels_, sizeof(channels_));
        sendto(tunnel_socket_, status, sizeof(status), 0, (struct sockaddr*) &tunnel_addr_, tunnel_addr_len_);
        
        // Check whether alsa harvester was succesful
        int is_success = 0;
        if (udp_handshake() < 0) {
            goto cleanup;
        }

        memcpy(&is_success, argv_, argv_size_);
        if (!is_success) {
            goto cleanup;
        }

        printf("Master initialized\n");

        // Start capturing
        alsa_recorder_run();
    }
    
    
cleanup:
    printf("Starting cleanup!\n");
    alsa_recorder_cleanup();
    return;    
}

void CLALSAHarvester::alsa_recorder_run() {
    int bytes_per_sample = snd_pcm_format_width(format_) / 8;
    int bytes_per_frame = bytes_per_sample * channels_;
    buffer_ = new char[buffer_frames_ * bytes_per_frame];
    int err;

    while (!tunnel_shutdown_flag_) {
        if ((err = snd_pcm_readi(capture_handle_, buffer_, buffer_frames_)) != buffer_frames_) {
            printf("Failed to read from audio interface (%s)\n", snd_strerror(err));
            return;
        }
        if (send_sample(buffer_, buffer_frames_ * bytes_per_frame) < 0) {
            return;
        }
    }
}

int CLALSAHarvester::send_sample(char* __buffer, int __size) {
    int rc = sendto(tunnel_socket_, __buffer, __size, 0, (struct sockaddr*) &tunnel_addr_, tunnel_addr_len_);
    if (rc < 0) {
        printf("Failed to send frame (%s)\n", strerror(errno));
        return -1;
    }
    if (__size == 0) {
        printf("Sent sample with size %d\n", __size);
        return -1;
    }

    return 0;
}

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
    hw_params_ = nullptr;

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

    for (int i=min_channels;i<=max_channels;i++) {
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
        return -ENOTSUP;
    }

    return 0;
}

void CLALSAHarvester::alsa_recorder_cleanup() {
    if (capture_handle_ != nullptr) {
        snd_pcm_close(capture_handle_);
        capture_handle_ = nullptr;
    }

    if (hw_params_ != nullptr) {
        snd_pcm_hw_params_free(hw_params_);
        hw_params_ = nullptr;
    }

    if (buffer_ != nullptr) {
        delete[] buffer_;
        buffer_ = nullptr;
    } 
}