#ifndef CLIENT_HANDLER_HPP
#define CLIENT_HANDLER_HPP

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <cstring>

class ClientHandler{
public:
    ClientHandler();
    static int is_client_up(const int __fd);
    void cleanup_client();
    void init_client(sockaddr_in __client_addr, int __client_socket, int __index);
    int socket() const;
    std::string ip() const;
    int index() const;
    int set_tunnel(uint8_t __tunnel);
    int unset_tunnel(uint8_t __tunnel);
private:
    void set_values();
    int socket_;
    std::string ip_;
    struct sockaddr_in addr_;
    socklen_t addr_len_;
    int index_;
    uint8_t active_tunnels_ = 0;
};

#endif // CLIENT_HANDLER_HPP