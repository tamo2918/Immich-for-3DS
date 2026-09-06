#include "Logger.hpp"
#include <cstdio>
#include <ctime>
#include <sys/stat.h>

std::string Logger::s_logPath = "sdmc:/3ds/Immich3DS/log.txt";
LogLevel Logger::s_minLevel = LogLevel::LOG_INFO;
std::string Logger::s_lastMessage = "";

void Logger::init(const std::string& logFilePath) {
    s_logPath = logFilePath;
    // Append a session separator
    FILE* f = fopen(s_logPath.c_str(), "a");
    if (f) {
        time_t now = time(nullptr);
        char timeStr[64];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(f, "\n=== Immich 3DS Session Started [%s] ===\n", timeStr);
        fclose(f);
    }
}

void Logger::close() {
    FILE* f = fopen(s_logPath.c_str(), "a");
    if (f) {
        fprintf(f, "=== Immich 3DS Session Ended ===\n");
        fclose(f);
    }
}

void Logger::setLevel(LogLevel level) {
    s_minLevel = level;
}

void Logger::log(LogLevel level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    logInternal(level, format, args);
    va_end(args);
}

void Logger::debug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    logInternal(LogLevel::LOG_DEBUG, format, args);
    va_end(args);
}

void Logger::info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    logInternal(LogLevel::LOG_INFO, format, args);
    va_end(args);
}

void Logger::warn(const char* format, ...) {
    va_list args;
    va_start(args, format);
    logInternal(LogLevel::LOG_WARN, format, args);
    va_end(args);
}

void Logger::error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    logInternal(LogLevel::LOG_ERROR, format, args);
    va_end(args);
}

std::string Logger::getLastLogMessage() {
    return s_lastMessage;
}

void Logger::logInternal(LogLevel level, const char* format, va_list args) {
    if (level < s_minLevel) return;

    const char* levelStr = "INFO";
    switch (level) {
        case LogLevel::LOG_DEBUG: levelStr = "DEBUG"; break;
        case LogLevel::LOG_INFO:  levelStr = "INFO "; break;
        case LogLevel::LOG_WARN:  levelStr = "WARN "; break;
        case LogLevel::LOG_ERROR: levelStr = "ERROR"; break;
    }

    char messageBuf[512];
    vsnprintf(messageBuf, sizeof(messageBuf), format, args);
    s_lastMessage = messageBuf;

    time_t now = time(nullptr);
    char timeStr[32];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", localtime(&now));

    // Print to stdout (console)
    printf("[%s] [%s] %s\n", timeStr, levelStr, messageBuf);

    // Append to file if path is set
    if (!s_logPath.empty()) {
        FILE* f = fopen(s_logPath.c_str(), "a");
        if (f) {
            fprintf(f, "[%s] [%s] %s\n", timeStr, levelStr, messageBuf);
            fclose(f);
        }
    }
}
