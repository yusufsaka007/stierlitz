#ifndef CLIENT_HANDLER_HPP
#define CLIENT_HANDLER_HPP

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <queue>
#include <sstream>

class ClientHandler{
public:
    ClientHandler();
    static int is_client_up(const int __fd);
    void cleanup_client();
    int get_client_socket() const;
    void init_client(sockaddr_in_ __client_addr, int __client_socket, int __index);
private:
    void set_values();
    int client_socket_;
    std::string ip_;
    struct sockaddr_in client_addr_;
    socklen_t addr_len_;
    int index_;
};

#endif // CLIENT_HANDLER_HPP