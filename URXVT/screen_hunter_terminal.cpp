#include <opencv2/opencv.hpp>
#include <atomic>
#include <thread>
#include <mutex>
#include "tunnel_terminal_include.hpp"

std::atomic<bool> shutdown_flag = false;
uint32_t width = 0;
uint32_t height = 0;
const int BUFFER_SIZE = 256;
std::string shell_str = "screen-hunter > ";

char end_key[16];
int end_key_len;

char res_update_key[16];
int res_update_key_len;

int fifo_in = -1;
int fifo_out = -1;
int fifo_data = -1;

unsigned char* rgb_data = nullptr;
int rgb_data_size;

std::mutex cleanup_mutex;

void cleanup() {
    if (fifo_in != -1) {
        close(fifo_in);
        fifo_in = -1;
    }
    if (fifo_out != -1) {
        close(fifo_out);
        fifo_out = -1;
    }
    if (fifo_data != -1) {
        close(fifo_data);
        fifo_data = -1;
    }
    if (rgb_data) {
        delete[] rgb_data;
    }
}

void handle_control(int __out_fifo) {
    fd_set read_fds;
    char buffer[BUFFER_SIZE + 1] = {0};
    size_t bytes_read = 0;
    while(!shutdown_flag.load()) {
        FD_ZERO(&read_fds);
        FD_SET(__out_fifo, &read_fds);

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int rc = select(__out_fifo + 1, &read_fds, nullptr, nullptr, &timeout);
        if (rc < 0) {
            std::cerr << RED << "[handle_control] Error in select()" << RESET << std::endl;
            shutdown_flag.store(true);
            break;
        } else if (rc == 0) {
            // Timeout 
            continue;
        }

        if (FD_ISSET(__out_fifo, &read_fds)) {
            bytes_read = read(__out_fifo, buffer, sizeof(buffer));
            std::cout << MAGENTA << "[handle_control] Data available for read:" <<  bytes_read << RESET << std::endl;
            if (bytes_read < 0) {
                std::cerr << RED << "[handle_control] Error reading from FIFO" << RESET << std::endl;
                break;
            } else if (bytes_read == 0) {
                std::cout << YELLOW << "[handle_control] FIFO closed by writer" << RESET << std::endl;
                break;
            } else {
                buffer[bytes_read] = '\0';
                if (bytes_read >= res_update_key_len + 8  && strncmp(buffer + (bytes_read-res_update_key_len), res_update_key, res_update_key_len) == 0) {
                    // Resolution update received.
                    uint32_t temp_width = 0, temp_height = 0;

                    memcpy(&temp_width, buffer, 4);
                    memcpy(&temp_height, buffer + 4, 4);

                    if (temp_width > 0 && temp_width <= 10000 && temp_height > 0 && temp_height <= 10000) {
                        width = temp_width;
                        height = temp_height;
                        rgb_data_size = width * height * 3 + end_key_len;
                        delete[] rgb_data;
                        rgb_data = new unsigned char(rgb_data_size);
                        printf("Resolution updated: %dx%d\n", width, height);
                    } else {
                        printf("Corrupted data received during resolution check\n\twidth=%d\n\theight=%d\n",temp_width, temp_height);
                    }
                    std::cout << YELLOW << shell_str << RESET << std::flush;
                    continue;
                }
                
                std::string line(buffer);
                if (line.find("[__end__]") != std::string::npos || line.find("[__error__]") != std::string::npos) {
                    shutdown_flag.store(true);
                    std::cout << RED << line << RESET << std::endl;
                    break;
                }

                std::cout << GREEN << line << "\n";
                std::cout << YELLOW << shell_str << RESET << std::flush;
            }
        }
    }
    {
        std::lock_guard<std::mutex> lock(cleanup_mutex);
        cleanup();
    }
}

void handle_data(int __data_fifo) {
    fd_set data_fds;
    rgb_data = new unsigned char[width * height * 3 + end_key_len];
    
    while (!shutdown_flag.load()) {
        FD_ZERO(&data_fds);
        FD_SET(__data_fifo, &data_fds);

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int rc = select(__data_fifo + 1, &data_fds, nullptr, nullptr, &timeout);
        if (rc < 0) {
            std::cerr << RED << "[handle_data] Error in select()" << RESET << std::endl;
            shutdown_flag.store(true);
            break;
        } else if (rc == 0) {
            // Timeout 
            continue;
        }

        if (FD_ISSET(__data_fifo, &data_fds)) {
            size_t total_read = 0;
            while (!shutdown_flag.load() && total_read < rgb_data_size) {
                size_t bytes_read = read(__data_fifo, rgb_data + total_read, rgb_data_size - total_read);
                if (bytes_read < 0) {
                    std::cerr << RED << "[handle_data] Error reading from FIFO" << RESET << std::endl;
                    break;
                } else if (bytes_read == 0) {
                    std::cout << YELLOW << "[handle_data] FIFO closed by writer" << RESET << std::endl;
                    break;
                } else {
                    std::cout << GREEN << "[handle_data] Received segment size: " << bytes_read << std::endl;
                    total_read += bytes_read;
                }
            }

            if (total_read == rgb_data_size && memcmp(rgb_data + (rgb_data_size - end_key_len), end_key, end_key_len) == 0) {
                std::cout << GREEN << "[handle_data] RGB data complete. Starting with appropriate functions" << std::endl;
            } else {
                std::cout << RED << "[handle_data] Corrupted RGB data received" << std::endl;
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(cleanup_mutex);
    }
}

int main(int argc, char* argv[]) {
    std::string fifo_name = argv[1]; // Control fifo READ_ONLY
    std::string fifo_in_name = fifo_name + (std::string) "_in"; // Command fifo WRITE_ONLY
    std::string fifo_data_name = fifo_name + (std::string) "_data"; // Data fifo

    end_key_len = std::stoi(argv[3]);
    strncpy(end_key, argv[2], end_key_len);
    end_key[end_key_len] = '\0';

    res_update_key_len = std::stoi(argv[5]);
    strncpy(res_update_key, argv[4], res_update_key_len);
    res_update_key[res_update_key_len] = '\0';

    std::filesystem::create_directories("fifo");
    if (!std::filesystem::exists(fifo_in_name)) {
        if (mkfifo(fifo_in_name.c_str(), 0666) == -1) {
            std::cerr << RED << "Error while creating the fifo" << RESET << std::endl;
            return -1;
        }
    }

    if (!std::filesystem::exists(fifo_name)) {
        if (mkfifo(fifo_name.c_str(), 0666) == -1) {
            std::cerr << RED << "Error while creating the fifo" << RESET << std::endl;
            return -1;
        }
    }

    // Open the read only fifos
    fifo_out = open(fifo_name.c_str(), O_RDONLY | O_NONBLOCK);
    if (fifo_out < 0) {
        std::cerr << RED << "Error while opening the fifo path: " << fifo_name << RESET << std::endl;
        return -1;
    }

    fifo_data = open(fifo_data_name.c_str(), O_RDONLY | O_NONBLOCK);
    if (fifo_out < 0) {
        std::cerr << RED << "Error while opening the fifo path: " << fifo_name << RESET << std::endl;
        return -1;
    }
    
    // Open the input fifo
    int tries = 0;
    while ((fifo_in = open(fifo_in_name.c_str(), O_WRONLY | O_NONBLOCK)) == -1 && tries <= 20) {
        std::cout << YELLOW << "Waiting for read end to connect..." << RESET;
        sleep(1);
    }

    if (fifo_in == -1) {
        std::cerr << RED << "Error opening input FIFO file" << RESET;
        return -1;
    }


    std::cout << GREEN << "[+] FIFOs Connected" << RESET << "\n";
    std::cout << GREEN << "\n\n====================Welcome to Screen Hunter====================\ntype help to see available commands\n" << RESET;

    std::thread control_handler(handle_control, fifo_out);
    std::thread data_handler(handle_data, fifo_data);

    // Test 
    std::string test_input = "";
    char buffer[1024];
    while (!shutdown_flag.load()) {
        std::cout << YELLOW << shell_str;
        std::getline(std::cin, test_input);
        if (fifo_in == -1) {
            break;
        }

        write(fifo_in, test_input.c_str(), test_input.size());
        
        if (strncmp(test_input.c_str(), "exit", 4) == 0) {
            shutdown_flag.store(true);
            break;
        }
    }

    if (control_handler.joinable()) {
        control_handler.join();
    }

    if (data_handler.joinable()) {
        data_handler.join();
    }

    std::cout << MAGENTA << "Code finished" << std::endl;

    {
        std::lock_guard<std::mutex> lock(cleanup_mutex);
        cleanup();
    }

    return 0;
}