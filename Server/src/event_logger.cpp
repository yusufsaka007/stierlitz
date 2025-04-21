#include "event_logger.hpp"

EventLog::EventLog(
    std::mutex* __log_mutex, 
    std::condition_variable* __log_cv, 
    std::queue<std::string>* __p_log_queue, 
    int* __p_user_verbosity
) {
    p_log_mutex_ = __log_mutex;
    p_log_cv_ = __log_cv;
    p_log_queue_ = __p_log_queue;
    p_user_verbosity_ = __p_user_verbosity;
    log_stream_.str("");
    log_stream_.clear();
}

EventLog& EventLog::operator<<(LogLevel level) {
    log_verbosity_ = level;
    return *this;
}

EventLog& EventLog::operator<<(const char* value) {
    if (log_verbosity_ < *p_user_verbosity_) {
        return *this;
    }
    if (std::string(value) == RESET || std::string(value) == RESET_NO_NEWLINE) {
        {
            log_stream_ << value;
            std::lock_guard<std::mutex> lock(*p_log_mutex_);
            p_log_queue_->push(log_stream_.str());
            p_log_cv_->notify_one();
        }
        log_stream_.str("");
        log_stream_.clear();
        log_verbosity_ = 0;
    } else {
        log_stream_ << value;
    }
    return *this;
}

EventLog& EventLog::operator<<(const std::string& value) {
    if (log_verbosity_ < *p_user_verbosity_) {
        return *this;
    }
    if (value == RESET || value == RESET_NO_NEWLINE) {
        {
            log_stream_ << value;
            std::lock_guard<std::mutex> lock(*p_log_mutex_);
            p_log_queue_->push(log_stream_.str());
            p_log_cv_->notify_one();
        }
        log_stream_.str("");
        log_stream_.clear();
        log_verbosity_ = 0;
    } else {
        log_stream_ << value;
    }
    return *this;
}