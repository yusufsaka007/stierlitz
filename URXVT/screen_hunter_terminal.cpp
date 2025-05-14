#include <opencv2/opencv.hpp>
#include <atomic>
#include <thread>
#include "tunnel_terminal_include.hpp"

#define END_KEY "$^_end"
#define END_KEY_LEN (sizeof(END_KEY) - 1)

std::atomic<bool> shutdown_flag = false;
std::string shell_str = "screen-hunter > ";

void reader_thread(int __out_fifo) {
    fd_set read_fds;
    char buffer[1024] = {0};
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
            shutdown_flag = true;
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
                std::cout << GREEN << "[+] Received size: " << bytes_read << RESET << std::endl;
                
                if (strncmp(buffer + (bytes_read-END_KEY_LEN), END_KEY, END_KEY_LEN)) {
                    std::cout << "Image data received" << std::endl;    
                }
                
                if (bytes_read < 256) {
                    std::string line(buffer);
                    if (line.find("[__end__]") != std::string::npos || line.find("[__error__]") != std::string::npos) {
                        // Server shuts down or an error ocurred
                        shutdown_flag = true;
                        std::cout << RED << line << RESET << std::endl;
                        break;
                    } 

                    std::cout << GREEN << line << std::endl;

                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    std::string fifo_name = argv[1]; // Data fifo READ_ONLY
    std::string fifo_in_name = fifo_name + (std::string) "_in"; // Command fifo WRITE_ONLY

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

    std::thread reader(reader_thread, fifo_out);

    // Test 
    std::string test_input = "";
    char buffer[1024];
    while (!shutdown_flag.load()) {
        std::cout << YELLOW << shell_str; 
        std::getline(std::cin, test_input);
        test_input += '\n';
        write(fifo_in, test_input.c_str(), test_input.size());
        if (strncmp(test_input.c_str(), "exit", 4) == 0) {
            shutdown_flag.store(true);
            break;
        }
        read(fifo_out, buffer, sizeof(buffer));
        std::string line(buffer);
        if (line.find("[__end__]") != std::string::npos) {
            std::cout << YELLOW << "Shutdown received\n[__end__]" << RESET << std::endl;
            break;
        }
        std::cout << line << std::endl;
    }

    if (reader.joinable()) {
        reader.join();
    }

    close(fifo_in);
    close(fifo_out);

    return 0;
}