/**
 * @file Server/main.cpp
 * @author viv4ldi
 * @brief Main file for the server
 * @version 0.1
 */

#include "server.hpp"

std::unique_ptr<Server> server;

static void sigterm_handler(int signum) {
    server->shutdown();
}

int main(int argc, char* argv[]) {
    int rc = 0;
    struct sigaction sa;
    sa.sa_handler = sigterm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    if (argc == 3) {
        server = std::make_unique<Server>(argv[1], atoi(argv[2]), atoi(argv[5]));
    } else if (argc == 1) {
        server = std::make_unique<Server>();
    } else {
        std::cerr << YELLOW << "Usage: " << argv[0] << " <ip> <port> <max_connections>" << RESET << std::endl;
        return 0;
    }

    rc = server->init();
    if (rc < 0) {
        return -1;
    }

    server->start();
    
    return 0;
}
