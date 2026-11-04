#pragma once

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>

namespace openroute::common {

enum class LogLevel {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

inline const char* LogLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
        default: return "UNKNOWN";
    }
}

class Logger {
public:
    static Logger& Instance() {
        static Logger instance;
        return instance;
    }

    void SetMinLevel(LogLevel level) {
        std::lock_guard<std::mutex> lock(mutex_);
        min_level_ = level;
    }

    LogLevel GetMinLevel() const {
        return min_level_;
    }

    void SetLogFile(const std::string& filepath) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_stream_.is_open()) {
            file_stream_.close();
        }
        file_stream_.open(filepath, std::ios::app);
    }

    void Log(LogLevel level, const char* file, int line, const std::string& message) {
        if (level < min_level_) {
            return;
        }

        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::tm tm_buf{};
#if defined(_WIN32)
        localtime_s(&tm_buf, &in_time_t);
#else
        localtime_r(&in_time_t, &tm_buf);
#endif

        std::ostringstream ss;
        ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
           << '.' << std::setfill('0') << std::setw(3) << ms.count()
           << " [" << LogLevelToString(level) << "] "
           << "[" << ExtractFilename(file) << ":" << line << "] "
           << message << "\n";

        std::string log_entry = ss.str();

        std::lock_guard<std::mutex> lock(mutex_);
        if (level >= LogLevel::Warn) {
            std::cerr << log_entry;
        } else {
            std::cout << log_entry;
        }

        if (file_stream_.is_open()) {
            file_stream_ << log_entry;
            file_stream_.flush();
        }
    }

private:
    Logger() : min_level_(LogLevel::Info) {}
    ~Logger() {
        if (file_stream_.is_open()) {
            file_stream_.close();
        }
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static std::string_view ExtractFilename(const char* path) {
        std::string_view sv(path);
        auto pos = sv.find_last_of("/\\");
        return (pos == std::string_view::npos) ? sv : sv.substr(pos + 1);
    }

    std::mutex mutex_;
    LogLevel min_level_;
    std::ofstream file_stream_;
};

class LogMessage {
public:
    LogMessage(LogLevel level, const char* file, int line)
        : level_(level), file_(file), line_(line) {}

    ~LogMessage() {
        Logger::Instance().Log(level_, file_, line_, stream_.str());
    }

    template <typename T>
    LogMessage& operator<<(const T& val) {
        stream_ << val;
        return *this;
    }

private:
    LogLevel level_;
    const char* file_;
    int line_;
    std::ostringstream stream_;
};

} // namespace openroute::common

#define LOG_TRACE ::openroute::common::LogMessage(::openroute::common::LogLevel::Trace, __FILE__, __LINE__)
#define LOG_DEBUG ::openroute::common::LogMessage(::openroute::common::LogLevel::Debug, __FILE__, __LINE__)
#define LOG_INFO  ::openroute::common::LogMessage(::openroute::common::LogLevel::Info,  __FILE__, __LINE__)
#define LOG_WARN  ::openroute::common::LogMessage(::openroute::common::LogLevel::Warn,  __FILE__, __LINE__)
#define LOG_ERROR ::openroute::common::LogMessage(::openroute::common::LogLevel::Error, __FILE__, __LINE__)
#define LOG_FATAL ::openroute::common::LogMessage(::openroute::common::LogLevel::Fatal, __FILE__, __LINE__)
