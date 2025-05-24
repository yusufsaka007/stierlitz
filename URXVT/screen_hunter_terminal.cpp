#include <atomic>
#include <thread>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <jpeglib.h>
#include <vector>
#include "tunnel_terminal_include.hpp"

#define CONTROL_BUFFER_SIZE 256

std::atomic<bool> shutdown_flag = false;
uint32_t width = 0;
uint32_t height = 0;

char end_key[16];
int end_key_len;

char res_update_key[32];
int res_update_key_len;

int fifo_in = -1;
int fifo_out = -1;
int fifo_data = -1;

char* res_data = nullptr;
int res_data_size = 0;
unsigned char* rgb_data = nullptr;
int rgb_data_size = 0;
int time_to_sleep_ms = 0;

void cleanup() {
    if (fifo_data != -1) {
        close(fifo_data);
        fifo_data = -1;
    }
    if (rgb_data) {
        delete[] rgb_data;
    }
    if (res_data) {
        delete[] res_data;
    }
}

void rgb_to_jpeg_mem(unsigned char* __rgb_data, unsigned char** __jpeg_data, unsigned long* __jpeg_size, int quality=50) {
    jpeg_compress_struct cinfo;
    jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    jpeg_mem_dest(&cinfo, __jpeg_data, __jpeg_size);

    // Image parameters
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3; // RGB
    cinfo.in_color_space =  JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    jpeg_start_compress(&cinfo, TRUE);
    JSAMPROW row_pointer;
    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer = &rgb_data[cinfo.next_scanline * width * 3];
        jpeg_write_scanlines(&cinfo, &row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
}

void handle_data() {
    fd_set data_fds;
    rgb_data_size = (width * height * 3) + end_key_len;
    rgb_data = new unsigned char[rgb_data_size];
    char control_buffer[CONTROL_BUFFER_SIZE];
    memset(control_buffer, 0, CONTROL_BUFFER_SIZE);
    
    while (!shutdown_flag.load()) {
        FD_ZERO(&data_fds);
        FD_SET(fifo_data, &data_fds);

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = time_to_sleep_ms * 1000;

        int rc = select(fifo_data + 1, &data_fds, nullptr, nullptr, &timeout);
        if (rc < 0) {
            std::cerr << RED << "[handle_data] Error in select()" << RESET << std::endl;
            shutdown_flag.store(true);
            break;
        } else if (rc == 0) {
            std::cerr << RED << "[handle_data] Timeout occured!: " << RESET << std::endl;
            break;
        }

        if (FD_ISSET(fifo_data, &data_fds)) {
            size_t total_read = 0;

            // Initial read for width and height to check whether it is changed
            ssize_t bytes_read = read(fifo_data, res_data, res_data_size);
            if (bytes_read < 0) {
                std::cerr << RED << "[handle_data] Error reading initial data from FIFO: " << strerror(errno) << RESET << std::endl;
                break;
            } else if (bytes_read == 0) {
                std::cout << YELLOW << "[handle_data] FIFO closed by writer" << RESET << std::endl;
                break;
            } else {
                if (bytes_read == res_data_size && memcmp(res_data + (res_data_size - res_update_key_len), res_update_key, res_update_key_len) == 0) {
                    // Received data is rgb data
                    uint32_t tmp_width;
                    uint32_t tmp_height;

                    memcpy(&tmp_width, res_data, sizeof(uint32_t));
                    memcpy(&tmp_height, res_data + sizeof(uint32_t), sizeof(uint32_t));
                    std::cout << MAGENTA << tmp_width << "x" << tmp_height << RESET << std::endl;
                    if (tmp_width != width || tmp_height != height) {
                        width = tmp_width;
                        height = tmp_height;

                        rgb_data_size = tmp_width * tmp_height * 3 + end_key_len;
                        // Resize if changed
                        delete[] rgb_data;
                        rgb_data = new unsigned char[rgb_data_size];
                    }

                    fd_set rgb_fds;
                    while (!shutdown_flag.load() && total_read < rgb_data_size) {
                        FD_ZERO(&rgb_fds);
                        FD_SET(fifo_data, &rgb_fds);
                        
                        struct timeval tv; // Inner timeout
                        tv.tv_sec = 3;
                        tv.tv_usec = 0;

                        rc = select(fifo_data + 1, &rgb_fds, nullptr, nullptr, &tv);
                        if (rc < 0) {
                            std::cerr << RED << "[handle_data] Error during select: " << strerror(errno) << RESET << std::endl;
                            shutdown_flag.store(true);
                            break;
                        } else if (rc == 0) {
                            std::cerr << RED << "[handle_data] Timeout occured!: " << RESET << std::endl;
                            shutdown_flag.store(true);
                            break;
                        }

                        bytes_read = read(fifo_data, rgb_data + total_read, rgb_data_size - total_read);
                        if (bytes_read < 0) {
                            std::cerr << RED << "[handle_data] Error reading from FIFO: " << strerror(errno) << RESET << std::endl;
                            shutdown_flag.store(true);
                            break;
                        } else if (bytes_read == 0) {
                            std::cout << YELLOW << "[handle_data] FIFO closed by writer" << RESET << std::endl;
                            shutdown_flag.store(true);
                            break;
                        } else {
                            if (bytes_read < 256) {
                                // Might be a message from the server
                                std::string control_data_final = std::string(rgb_data + total_read, rgb_data + total_read + bytes_read);
                                if (control_data_final.find("[__error__]") != std::string::npos) {
                                    std::cout << "\n" << RED << control_data_final << RESET << std::endl;
                                    shutdown_flag.store(true);
                                    break;
                                }
                            }
                            printf("[handle_data] Received segment size: %d\n", bytes_read);
                            total_read += bytes_read;
                        }
                    }
                    if (shutdown_flag.load()) {
                        std::cout << YELLOW << "[handle_data] Terminating..." << RESET << std::endl;
                        break;
                    }
                    std::cout << MAGENTA << "[handle_data] Total received: " << total_read << RESET << std::endl; 

                    if (total_read == rgb_data_size && memcmp(rgb_data + (rgb_data_size - end_key_len), end_key, end_key_len) == 0) {
                        std::cout << GREEN << "[handle_data] RGB data complete. Starting with appropriate functions." << RESET << std::endl;

                        unsigned char* jpeg_data;
                        unsigned long jpeg_size = 0;

                        rgb_to_jpeg_mem(rgb_data, &jpeg_data, &jpeg_size, 70);
                        printf("[handle_data] JPEG size size: %ld\n", jpeg_size);

                        std::vector<unsigned char> buffer(jpeg_data, jpeg_data + jpeg_size);
                        cv::Mat img = cv::imdecode(buffer, cv::IMREAD_COLOR);
                        free(jpeg_data);
                        if (img.empty()) {
                            printf("[handle_data] Image is empty!\n");
                        } else {
                            uint32_t target_width = 1920;
                            uint32_t target_height = 1080;
                            uint32_t original_width = img.cols;
                            uint32_t original_height = img.rows;

                            double scale_w = (double) target_width / original_width;
                            double scale_h = (double) target_height / original_height;
                            double scale = std::min(scale_w, scale_h); // Aspect ratio

                            int new_width = (int) (original_width * scale);
                            int new_height = (int) (original_height * scale);

                            cv::resize(img, img, cv::Size(new_width, new_height), 0, 0, cv::INTER_AREA);
                            cv::imshow("Screen Hunter", img);
                            cv::waitKey(1);
                        }
                    } else {
                        std::cout << RED << "[handle_data] Corrupted RGB data received" << RESET << std::endl;    
                    }
                } else {
                    // Received data is control data (from server)
                    // Copy the initial receive
                    strncpy(control_buffer, res_data, bytes_read);
                    // Receive the remaining (if any)
                    bytes_read = read(fifo_data, control_buffer, CONTROL_BUFFER_SIZE - bytes_read);
                    if (bytes_read <= 0) {
                        std::cerr << RED << "[handle_data] Error reading the remaining control data from FIFO: " << strerror(errno) << RESET << std::endl;
                        break;
                    }

                    std::string control_data_final = (std::string) control_buffer;
                    if (control_data_final.find("[__end__]") != std::string::npos) {
                        std::cout << "\n" << YELLOW << "[__end__]" << RESET << std::endl;;
                        break;
                    }
                    if (control_data_final.find("[__error__]") != std::string::npos) {
                        std::cout << "\n" << RED << control_data_final << RESET << std::endl;
                        break;
                    }
                }
            }
        }
    }

    cv::destroyAllWindows();
}

int main(int argc, char* argv[]) {
    std::string fifo_data_name = argv[1]; // Control fifo READ_ONLY

    end_key_len = std::stoi(argv[3]);
    strncpy(end_key, argv[2], end_key_len);
    end_key[end_key_len] = '\0';

    res_update_key_len = std::stoi(argv[5]);
    strncpy(res_update_key, argv[4], res_update_key_len);
    res_update_key[res_update_key_len] = '\0';

    res_data_size = sizeof(width) + sizeof(height) + res_update_key_len;
    res_data = new char[res_data_size];

    time_to_sleep_ms = std::stoi(argv[6]);

    std::filesystem::create_directories("fifo");
     if (!std::filesystem::exists(fifo_data_name)) {
        if (mkfifo(fifo_data_name.c_str(), 0666) == -1) {
            std::cerr << RED << "Error while creating the fifo" << RESET << std::endl;
            return -1;
        }
    }

    fifo_data = open(fifo_data_name.c_str(), O_RDONLY);
    if (fifo_data < 0) {
        std::cerr << RED << "Error while opening the fifo path: " << fifo_data_name << RESET << std::endl;
        return -1;
    }

    std::cout << GREEN << "[+] FIFO Connected" << RESET << "\n";
    std::cout << GREEN << "\n\n====================Welcome to Screen Hunter====================\n" << RESET;

    handle_data();

    std::cout << MAGENTA << "Code finished" << std::endl;

    std::filesystem::remove(fifo_data_name);

    cleanup();

    return 0;
}