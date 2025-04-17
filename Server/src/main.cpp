/**
 * @file Server/main.cpp
 * @author viv4ldi
 * @brief Main file for the server
 * @version 0.1
 */

#include "server.hpp"

std::unique_ptr<Server> server;

static void sigterm_handler(int signum) {
    std::cerr << YELLOW << "[main] Received signal " << signum << ", stopping server" << RESET << std::endl;
    server->shutdown();
}

int main() {
    int rc = 0;
    struct sigaction sa;
    sa.sa_handler = sigterm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    if (argc == 3) {
        server = std::make_unique<Server>(atoi(argv[1]), argv[2], argv[3], argv[4], atoi(argv[5]));
    } else if (argc == 1) {
        server = std::make_unique<Server>();
    } else {
        std::cerr << YELLOW << "Usage: " << argv[0] << " <port> <ip> <username> <password> <max_connections>" << RESET << std::endl;
        return 0;
    }

    rc = server->init();
    if (rc < 0) {
        return -1;
    }

    server->start();
    
    return 0;
}
