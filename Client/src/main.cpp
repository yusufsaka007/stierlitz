#include "client.hpp"


int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <ip> <port>\n", argv[0]);
        return -1;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);

    Client client(ip, port);
    if (client.init() < 0) {
        printf("Failed to initialize client\n");
        return -1;
    }
    client.start();
    client.shutdown();

    return 0;
}