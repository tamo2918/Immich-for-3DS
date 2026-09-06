#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <cstdarg>

enum class LogLevel {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
};

class Logger {
public:
    static void init(const std::string& logFilePath);
    static void close();
    static void setLevel(LogLevel level);
    
    static void log(LogLevel level, const char* format, ...);
    static void debug(const char* format, ...);
    static void info(const char* format, ...);
    static void warn(const char* format, ...);
    static void error(const char* format, ...);

    static std::string getLastLogMessage();

private:
    static std::string s_logPath;
    static LogLevel s_minLevel;
    static std::string s_lastMessage;
    static void logInternal(LogLevel level, const char* format, va_list args);
};

#endif // LOGGER_HPP
