#include <opencv2/opencv.hpp>
#include "tunnel_terminal_include.hpp"

volatile bool shutdown_flag_ = false;

void handle_sigint(int) {
    shutdown_flag_ = true;
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, handle_sigint);
    std::string fifo_name = argv[1];
    std::string out_name = argv[2];

    int width = 160;
    int height = 120;
    int frame_size = width*height+4096;

    std::filesystem::create_directories("fifo");
    if (!std::filesystem::exists(fifo_name)) {
        if (mkfifo(fifo_name.c_str(), 0666) == -1) {
            std::cerr << RED << "[-] Error while creating the fifo" << RESET << std::endl;
            return -1;
        }
    }

    int fd = open(fifo_name.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        std::cerr << RED << "[-] Error while opening the fifo path: " << fifo_name << RESET << std::endl;
        return -1;
    }

    int out_fd = -1;
    if (!out_name.empty()) {
        open(out_name.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (out_fd < 0) {
            std::cerr << RED << "[-] Error opening " << out_name << RESET << std::endl;
        }
    }

    std::cout << BLUE << "[+] Connected to the output FIFO" << RESET << "\n";
    std::cout << BLUE << "\n\n====================Listening For Frames====================\n\n" << RESET;

    

    fd_set read_fds;
    char buffer[frame_size + 1];    
    while (!shutdown_flag_) {
        FD_ZERO(&read_fds);
        FD_SET(fd, &read_fds);

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;

        int rc = select(fd + 1, &read_fds, nullptr, nullptr, &timeout);
        if (rc > 0 && FD_ISSET(fd, &read_fds)) {
            ssize_t bytes_read = read(fd, buffer, frame_size);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                if (bytes_read > 256) {
                    std::cout << RED <<  "Received frame size: " << bytes_read << RESET << std::endl;
                    std::vector<unsigned char> jpeg_data(buffer, buffer+bytes_read);
                    cv::Mat img = cv::imdecode(jpeg_data, cv::IMREAD_COLOR);
                    if (img.empty()) {
                        std::cout << RED << "Image is empty" << std::endl;                        
                    }
                    if (out_fd >= 0) {
                        rc = write(out_fd, buffer, bytes_read); // ffmpeg -f mjpeg -i out.mjpeg -c:v copy out.avi
                        if (rc < 0) {
                            std::cerr << RED << "Error writing to output file: " << strerror(errno) << std::endl;
                            close(out_fd);
                            out_fd = -1; 
                        
                        }
                    }
                    cv::imshow("Live", img);
                    cv::waitKey(1);
                }
                std::string line(buffer);
                if (line.find("[__end__]") != std::string::npos) {
                    std::cout << "\n" << MAGENTA << "[__end__]" << RESET << std::endl;;
                    break;
                }
                if (line.find("[__error__]") != std::string::npos) {
                    std::cout << "\n" << RED << line << RESET << std::endl;
                    break;
                }
                

            } else {
                std::cout << "\n" << RED << "Error occured while reading" << RESET << std::endl;
                break;
            }
        } else if (rc < 0) {
            std::cerr << RED << "An error occurred while reading from fifo: " << strerror(errno) << RESET << std::endl;
            break;
        }
    }

    std::cout << RESET;
    cv::destroyAllWindows();
    close(fd);
    std::filesystem::remove(fifo_name);

    std::cout << YELLOW << "[!] Press Enter to exit..." << RESET << "\n";
    std::cin.get();

    system("exit");

}