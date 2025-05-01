#include "client.hpp"
#include <signal.h>

Client* client = nullptr;

void sigterm_handler(int signum) {
    if (client != nullptr) {
        client->shutdown();
    }

    delete client;
}


int main(int argc, char* argv[]) {
    struct sigaction sa;
    sa.sa_handler = sigterm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    
    if (argc != 3) {
        printf("Usage: %s <ip> <port>\n", argv[0]);
        return -1;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);

    client = new Client(ip, port);
    if (client->init() < 0) {
        printf("Failed to initialize client\n");
        return -1;
    }
    client->start();
    client->shutdown();

    delete client;

    return 0;
}