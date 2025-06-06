#include "client.hpp"
#include <signal.h>
#include "debug.hpp"

Client* client = nullptr;

void sigterm_handler(int signum) {
    if (client != nullptr) {
        client->shutdown();
    }

}


int main(int argc, char* argv[]) {
    signal(SIGPIPE, SIG_IGN);
    struct sigaction sa;
    sa.sa_handler = sigterm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    
    if (argc != 3) {
        DEBUG_PRINT("Usage: %s <ip> <port>\n", argv[0]);
        return -1;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);

    client = new Client(ip, port);
    if (client->init() < 0) {
        DEBUG_PRINT("Failed to initialize client\n");
        return -1;
    }
    client->start();

    if (client) {
        delete client;
    }
    return 0;
}