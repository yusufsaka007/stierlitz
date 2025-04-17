#ifndef DATA_HANDLER_HPP
#define DATA_HANDLER_HPP

#include <iostream>
#include <atomic>

int recv_buf(int __fd, char* __buf, int __len);
#ifdef SERVER
int recv_buf(int __fd, char* __buf, int __len, const std::atomic<bool>& __shutdown_flag);
#endif // SERVER

int send_buf(int __fd, const char* __buf, int __len);

#endif // DATA_HANDLER_HPP