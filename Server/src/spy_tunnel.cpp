#include "spy_tunnel.hpp"

SpyTunnel::SpyTunnel(std::string* __p_ip, uint* __p_port, int __client_index, std::atomic<bool>* __p_shutdown_flag, int __connection_type) {    
    p_ip_ = __p_ip;
    p_port_ = __p_port;
    client_index_ = __client_index;
    p_shutdown_flag_ = __p_shutdown_flag;
    socket_ = -1;
    connection_type_ = __connection_type;
}

int SpyTunnel::run() {
    int comm_pipe[2];
    if (pipe(comm_pipe) == -1) {
        return -1;
    }
    
    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    } else if (pid == 0) {
        // Child process
        close(comm_pipe[1]); // Child-end of the pipe is read-only
        dup2(comm_pipe[0], STDIN_FILENO);
        close(comm_pipe[0]);

        spawn_window();
        std::cerr << RED << "[Server::handle_c2] Error executing C2 script: " << strerror(errno) << RESET;
        _exit(EXIT_FAILURE);
    }


    close(comm_pipe[0]);
    std::string success_msg = "SpyTunnel started successfully";
    int rc;
    socket_ = socket(AF_INET, connection_type_, 0);
    if (socket_ == -1) {
        const char* error_msg = "Socket creation failed";
        write(comm_pipe[1], error_msg, strlen(error_msg));
        goto cleanup;    
    }
    memset(&server_addr_, 0, sizeof(server_addr_));
    server_addr_.sin_family = AF_INET;
    server_addr_.sin_port = htons(*p_port_);
    if (inet_pton(AF_INET, p_ip_->c_str(), &server_addr_.sin_addr) <= 0) {
        const char* error_msg = "Invalid address or address not supported";
        write(comm_pipe[1], error_msg, strlen(error_msg));
        goto cleanup;
    }

    rc = bind(socket_, (const struct sockaddr*) &server_addr_, sizeof(server_addr_));
    if (rc < 0) {
        const char* error_msg = "Error binding socket";
        write(comm_pipe[1], error_msg, strlen(error_msg));
        goto cleanup;
    }
    rc = listen(socket_, 1);
    if (rc < 0) {
        const char* error_msg = "Error listening on socket";
        write(comm_pipe[1], error_msg, strlen(error_msg));
        goto cleanup;
    }
    rc = accept(socket_, nullptr, nullptr);
    if (rc < 0) {
        const char* error_msg = "Error accepting connection";
        write(comm_pipe[1], error_msg, strlen(error_msg));
        goto cleanup;
    }
    write(comm_pipe[1], success_msg.c_str(), success_msg.length());
    close(comm_pipe[1]);
    handle_tunnel(rc, comm_pipe[1]);
    close(rc);
    
cleanup:
    if (socket_ != -1) {
        close(socket_);
    }
    if (comm_pipe[0] != -1) {
        close(comm_pipe[0]);
    }
    if (comm_pipe[1] != -1) {
        close(comm_pipe[1]);
    }
    if (pid > 0) {
        kill(pid, SIGTERM);
        waitpid(pid, nullptr, 0);
    }
    return 0;
}