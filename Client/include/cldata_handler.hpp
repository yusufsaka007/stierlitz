#ifndef CLDATA_HANDLER_HPP
#define CLDATA_HANDLER_HPP

#include "cltunnel_include.hpp"

int send_out(int __fd, Status __status, sockaddr* __p_tunnel_addr=nullptr, socklen_t* __p_tunnel_addr_len=nullptr);

#endif // CLDATA_HANDLER_HPP