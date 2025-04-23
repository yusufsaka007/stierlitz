#include "event_logger.hpp"

EventLog::EventLog(std::shared_ptr<LogContext> context)
    : log_context_(std::move(context)) {
    log_stream_.str("");
    log_stream_.clear();
}

EventLog& EventLog::operator<<(LogLevel level) {
    log_verbosity_ = level;
    return *this;
}

EventLog& EventLog::operator<<(const char* value) {
    if (log_verbosity_ > *log_context_->p_user_verbosity_) {
        return *this;
    }
    if (std::string(value) == RESET || std::string(value) == RESET_NO_NEWLINE) {
        {
            log_stream_ << value;
            std::lock_guard<std::mutex> lock(log_context_->log_mutex_);
            log_context_->log_queue_.push(log_stream_.str());
            log_context_->log_cv_.notify_one();
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
    if (log_verbosity_ > *log_context_->p_user_verbosity_) {
        return *this;
    }
    if (value == RESET || value == RESET_NO_NEWLINE) {
        {
            log_stream_ << value;
            std::lock_guard<std::mutex> lock(log_context_->log_mutex_);
            log_context_->log_queue_.push(log_stream_.str());
            log_context_->log_cv_.notify_one();
        }
        log_stream_.str("");
        log_stream_.clear();
        log_verbosity_ = 0;
    } else {
        log_stream_ << value;
    }
    return *this;
}