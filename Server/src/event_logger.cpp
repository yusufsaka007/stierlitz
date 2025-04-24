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
        log_stream_.str("");
        log_stream_.clear();
        return *this;
    }
    if (std::string(value) == RESET || std::string(value) == RESET_NO_NEWLINE) {
        {
            log_stream_ << value;
            std::lock_guard<std::mutex> lock(log_context_->log_mutex_);
            log_context_->log_queue_.push(log_stream_.str());
            log_context_->log_cv_.notify_one();
        }
        reset();
    } else if(std::string(value) == RESET_C2_FIFO) {
        log_stream_ << value;
        std::string log_message = log_stream_.str();
        if (*log_context_->p_c2_fifo_ >= 0) {
            ssize_t bytes_written = write(*log_context_->p_c2_fifo_, log_message.c_str(), log_message.size());
            if (bytes_written < 0) {
                c2_fifo_error();
            }
        } else {
            c2_fifo_error();   
        }
        reset();
        
    } else {
        log_stream_ << value;
    }
    return *this;
}

EventLog& EventLog::operator<<(const std::string& value) {
    if (log_verbosity_ > *log_context_->p_user_verbosity_) {
        log_stream_.str("");
        log_stream_.clear();
        return *this;
    }
    if (value == RESET || value == RESET_NO_NEWLINE) {
        {
            log_stream_ << value;
            std::lock_guard<std::mutex> lock(log_context_->log_mutex_);
            log_context_->log_queue_.push(log_stream_.str());
            log_context_->log_cv_.notify_one();
        }
        reset();
    } else if(value == RESET_C2_FIFO) {
        log_stream_ << value;
        std::string log_message = log_stream_.str();
        if (*log_context_->p_c2_fifo_ >= 0) {
            ssize_t bytes_written = write(*log_context_->p_c2_fifo_, log_message.c_str(), log_message.size());
            if (bytes_written < 0) {
                c2_fifo_error();
            }
        } else {
            c2_fifo_error();   
        }
        reset();
        
    } else {
        log_stream_ << value;
    }
    return *this;
}

void EventLog::reset() {
    log_stream_.str("");
    log_stream_.clear();
    log_verbosity_ = 0;
}

void EventLog::c2_fifo_error() {
    {
        std::lock_guard<std::mutex> lock(log_context_->log_mutex_);
        reset();
        log_stream_ << RED << "[EventLog] Error writing to " << C2_IN_FIFO_PATH << ": " << strerror(errno) << RESET;
        log_context_->log_queue_.push(log_stream_.str());
        log_context_->log_cv_.notify_one();
    }
}