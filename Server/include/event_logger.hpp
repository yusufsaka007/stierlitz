#ifndef EVENT_LOGGER_HPP
#define EVENT_LOGGER_HPP

#include <iostream>
#include <sstream>
#include <condition_variable>
#include <mutex>
#include <queue>
#include "server_macros.hpp"

enum LogLevel {
    LOG_DEBUG = 0,
    LOG_MUST = 0,
    LOG_MINOR_EVENTS = 1,
    LOG_CRITICAL_EVENTS = 2,
    LOG_CRASHES = 3,
    LOG_NONE = 4
};

class EventLog {
public:
    EventLog(
        std::mutex* __log_mutex, 
        std::condition_variable* __log_cv, 
        std::queue<std::string>* __p_log_queue, 
        int* __p_user_verbosity
    );

    template <typename T>
    EventLog& operator<<(const T& value) {
        log_stream_ << value;
        return *this;
    }

    EventLog& operator<<(const char* value);
    EventLog& operator<<(const std::string& value);
    EventLog& operator<<(LogLevel level);
private:
    std::mutex* p_log_mutex_;
    std::condition_variable* p_log_cv_;
    std::queue<std::string>* p_log_queue_;
    int* p_user_verbosity_;
    int log_verbosity_ = 0; // Highest by default
    std::stringstream log_stream_;
};

#endif // EVENT_LOGGER_HPP