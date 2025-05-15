#include <opencv2/opencv.hpp>
#include <atomic>
#include <thread>
#include "tunnel_terminal_include.hpp"

std::atomic<bool> shutdown_flag = false;
uint32_t width = 0;
uint32_t height = 0;
int buffer_size = 1024;
std::string shell_str = "screen-hunter > ";

char end_key[16];
int end_key_len;

char res_update_key[16];
int res_update_key_len;

void data_handler(int __out_fifo) {
    fd_set read_fds;
    char buffer[buffer_size + 1] = {0};
    size_t bytes_read = 0;
    while(!shutdown_flag.load()) {
        FD_ZERO(&read_fds);
        FD_SET(__out_fifo, &read_fds);

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int rc = select(__out_fifo + 1, &read_fds, nullptr, nullptr, &timeout);
        if (rc < 0) {
            std::cerr << RED << "Error in select()" << RESET << std::endl;
            shutdown_flag.store(true);
            break;
        } else if (rc == 0) {
            // Timeout 
            continue;
        }

        if (FD_ISSET(__out_fifo, &read_fds)) {
            bytes_read = read(__out_fifo, buffer, sizeof(buffer));
            if (bytes_read < 0) {
                std::cerr << RED << "Error reading from FIFO" << RESET << std::endl;
                break;
            } else if (bytes_read == 0) {
                std::cout << YELLOW << "FIFO closed by writer" << RESET << std::endl;
                break;
            } else {
                buffer[bytes_read] = '\0';
                std::cout << "\33[2K\r";
                if (bytes_read >= end_key_len && strncmp(buffer + (bytes_read-end_key_len), end_key, end_key_len) == 0) {
                    // Frame received. Will be handled here
                    printf("Frame received with size %d\n", bytes_read);

                    std::cout << YELLOW << shell_str << RESET << std::flush;
                    continue;
                }
                if (bytes_read == res_update_key_len + 8  && strncmp(buffer + (bytes_read-res_update_key_len), res_update_key, res_update_key_len) == 0) {
                    // Resolution update received.
                    uint32_t temp_width = 0, temp_height = 0;

                    memcpy(&temp_width, buffer, 4);
                    memcpy(&temp_height, buffer + 4, 4);

                    if (temp_width > 0 && temp_width <= 10000 && temp_height > 0 && temp_height <= 10000) {
                        width = temp_width;
                        height = temp_height;
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
}

int main(int argc, char* argv[]) {
    std::string fifo_name = argv[1]; // Data fifo READ_ONLY
    std::string fifo_in_name = fifo_name + (std::string) "_in"; // Command fifo WRITE_ONLY

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

    // Open the output fifo first
    int fifo_out = open(fifo_name.c_str(), O_RDONLY);
    if (fifo_out < 0) {
        std::cerr << RED << "Error while opening the fifo path: " << fifo_name << RESET << std::endl;
        return -1;
    }

    // Open the input fifo
    int tries = 0;
    int fifo_in = -1;
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

    std::thread reader(data_handler, fifo_out);

    // Test 
    std::string test_input = "";
    char buffer[1024];
    std::cout << YELLOW << shell_str;
    while (!shutdown_flag.load()) {
        std::getline(std::cin, test_input);
        write(fifo_in, test_input.c_str(), test_input.size());
        
        if (strncmp(test_input.c_str(), "exit", 4) == 0) {
            shutdown_flag.store(true);
            break;
        }
    }

    if (reader.joinable()) {
        reader.join();
    }

    close(fifo_in);
    close(fifo_out);

    return 0;
}