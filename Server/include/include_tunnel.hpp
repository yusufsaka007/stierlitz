#ifndef INCLUDE_TUNNEL_HPP
#define INCLUDE_TUNNEL_HPP

#include <iostream>
#include <string>
#include <atomic>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <vector>
#include <sys/select.h>
#include <algorithm>
#include <mutex>
#include <condition_variable>
#include <queue>
#include "common.hpp"
#include "scoped_epoll.hpp"
#include "server_macros.hpp"

#endif // INCLUDE_TUNNEL_HPP