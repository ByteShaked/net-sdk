// logger.h
#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <ctime>
#include <memory>

namespace network_log {

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    static void Init(const std::string& path = "./logs", 
                    size_t max_size = 1048576,  // 1MB
                    int rotation_hours = 24);
    
    // static void Write(LogLevel level, const std::string& message);
    static void Write(LogLevel level, const char* format, ...);
    static void Shutdown();

private:
    class Impl;
    static std::unique_ptr<Impl> impl_;
};

} // namespace network_log

#endif