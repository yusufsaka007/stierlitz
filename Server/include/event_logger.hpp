#ifndef EVENT_LOGGER_HPP
#define EVENT_LOGGER_HPP

#include <iostream>
#include <sstream>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <unistd.h>
#include <cstring>
#include "server_macros.hpp"

enum LogLevel {
    LOG_NONE = 0,
    LOG_DEBUG = 4,
    LOG_MINOR_EVENTS = 3,
    LOG_CRITICAL_EVENTS = 2,
    LOG_CRASHES = 1,
    LOG_MUST = 0
};

struct LogContext {
    std::mutex log_mutex_;
    std::condition_variable log_cv_;
    std::queue<std::string> log_queue_;
    int* p_user_verbosity_ = nullptr;
    int* p_c2_fifo_ = nullptr;
};

class EventLog {
public:
    explicit EventLog(std::shared_ptr<LogContext> context);

    template <typename T>
    EventLog& operator<<(const T& value) {
        log_stream_ << value;
        return *this;
    }

    EventLog& operator<<(const char* value);
    EventLog& operator<<(const std::string& value);
    EventLog& operator<<(LogLevel level);
    void reset();
    void c2_fifo_error();
private:
    int log_verbosity_ = 0; // By default printed
    std::stringstream log_stream_;
    std::shared_ptr<LogContext> log_context_;
};

#endif // EVENT_LOGGER_HPP