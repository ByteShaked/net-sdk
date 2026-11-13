
#include "NetLog.h"
#include <sys/stat.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <hilog/log.h>
#include <stdarg.h>

#undef LOG_DOMAIN 
#undef LOG_TAG 
#define LOG_DOMAIN 0x3200 // 全局domain宏，标识业务领域 
#define LOG_TAG "MY_TAG"  // 全局tag宏，标识模块日志tag
namespace network_log {

class Logger::Impl {
public:
    Impl(const std::string& path, size_t max_size, int hours)
        : log_dir_(path), 
          max_size_(max_size),
          rotation_interval_(hours*3600),
          current_size_(0)
    {
        create_directory(log_dir_);
        rotate_file();
    }

    void write(LogLevel level, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        check_rotation();
        
        std::time_t t = std::time(nullptr);
        char time_str[20];
        std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        
        std::string log_entry = std::string(time_str) + " [" + level_to_str(level) + "] " + message + "\n";
        
        if (file_.is_open()) {
            file_ << log_entry;
            file_.flush();
            current_size_ += log_entry.size();
        }
    }

private:
    std::ofstream file_;
    std::mutex mutex_;
    std::string log_dir_;
    std::string current_path_;
    size_t max_size_;
    size_t current_size_;
    time_t rotation_interval_;
    time_t last_rotation_;

    void create_directory(const std::string& path) {
        struct stat st;
        if (stat(path.c_str(), &st) != 0) {
            #if defined(_WIN32)
            mkdir(path.c_str());
            #else 
            mkdir(path.c_str(), 0755);
            #endif
        }
        OH_LOG_INFO(LOG_APP, "path:  [%{public}s]", path.c_str());
    }

    std::string generate_filename() {
        std::time_t t = std::time(nullptr);
        char time_str[20];
        std::strftime(time_str, sizeof(time_str), "%Y%m%d_%H%M%S", std::localtime(&t));
         OH_LOG_INFO(LOG_APP, "log_dir_:  [%{public}s]", log_dir_.c_str());
        return log_dir_ + "/network_" + time_str + ".log";
    }

    void rotate_file() {
        if (file_.is_open()) {
            file_.close();
        }
        
        current_path_ = generate_filename();
        file_.open(current_path_, std::ios::app);
        current_size_ = 0;
        last_rotation_ = std::time(nullptr);
    }

    void check_rotation() {
        time_t now = std::time(nullptr);
        
        // 时间滚动检查
        if ((now - last_rotation_) > rotation_interval_) {
            rotate_file();
            return;
        }
        
        // 大小滚动检查
        if (current_size_ >= max_size_) {
            rotate_file();
        }
    }

    const char* level_to_str(LogLevel level) {
        switch(level) {
            case LogLevel::DEBUG:   return "DEBUG";
            case LogLevel::INFO:    return "INFO";
            case LogLevel::WARNING: return "WARN";
            case LogLevel::ERROR:   return "ERROR";
            default:                return "UNKNOWN";
        }
    }
};

// Static member initialization
std::unique_ptr<Logger::Impl> Logger::impl_;

void Logger::Init(const std::string& path, size_t max_size, int hours) {
    impl_.reset(new Impl(path, max_size, hours));
}

void Logger::Write(LogLevel level, const char* format, ...) {
    if (!impl_) return;

    va_list args;
    va_start(args, format);
    
    // 第一次调用获取长度
    int length = vsnprintf(nullptr, 0, format, args);
    va_end(args);
    
    if (length <= 0) return;
    
    // 分配缓冲区
    std::vector<char> buffer(length + 1);
    
    va_start(args, format);
    vsnprintf(buffer.data(), buffer.size(), format, args);
    va_end(args);
    
    if (impl_) {
        impl_->write(level, std::string(buffer.data()));
    }
}

void Logger::Shutdown() {
    if (impl_) {
        impl_.reset();
    }
}

} // namespace network_log