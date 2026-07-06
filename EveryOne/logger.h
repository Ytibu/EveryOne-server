#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <mutex>
#include <ctime>
#include <iomanip>

enum class LogLevel
{
    DEBUG,
    INFO,
    WARN,
    ERROR
};

class Logger
{
public:
    static Logger &GetInstance()
    {
        static Logger instance;
        return instance;
    }

    void SetLevel(LogLevel level) { m_level = level; }

    void Log(LogLevel level, const std::string &message)
    {
        if (level < m_level)
            return;

        std::lock_guard<std::mutex> lock(m_mutex);
        auto &os = (level >= LogLevel::ERROR) ? std::cerr : std::cout;
        os << "[" << LevelToString(level) << "] "
           << CurrentTimestamp() << " "
           << message << std::endl;
    }

    void Log(LogLevel level, const std::string &file, int line, const std::string &message)
    {
        if (level < m_level)
            return;

        std::lock_guard<std::mutex> lock(m_mutex);
        auto &os = (level >= LogLevel::ERROR) ? std::cerr : std::cout;
        os << "[" << LevelToString(level) << "] "
           << CurrentTimestamp() << " "
           << file << ":" << line << " "
           << message << std::endl;
    }

private:
    Logger() : m_level(LogLevel::DEBUG) {}
    ~Logger() = default;
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    static std::string LevelToString(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default:              return "UNKNOWN";
        }
    }

    static std::string CurrentTimestamp()
    {
        auto now = std::time(nullptr);
        auto tm = *std::localtime(&now);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    LogLevel m_level;
    std::mutex m_mutex;
};

#define LOG_DEBUG(msg)  Logger::GetInstance().Log(LogLevel::DEBUG, __FILE__, __LINE__, (msg))
#define LOG_INFO(msg)   Logger::GetInstance().Log(LogLevel::INFO,  __FILE__, __LINE__, (msg))
#define LOG_WARN(msg)   Logger::GetInstance().Log(LogLevel::WARN,  __FILE__, __LINE__, (msg))
#define LOG_ERROR(msg)  Logger::GetInstance().Log(LogLevel::ERROR, __FILE__, __LINE__, (msg))
