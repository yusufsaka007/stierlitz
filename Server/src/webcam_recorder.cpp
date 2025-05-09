#include "webcam_recorder.hpp"

void WebcamRecorder::run() {

}

void WebcamRecorder::spawn_window() {
    execlp("urxvt", "urxvt", "-hold", "-name", "stierlitz_wr", "-e", WEBCAM_RECORDER_SCRIPT_PATH, fifo_path_.c_str(), out_name_, (char*)NULL);
}