#ifndef BACKUP_COMMON_LOGGER_H
#define BACKUP_COMMON_LOGGER_H

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>
#include <string>

namespace backup {

/**
 * Logger singleton for the backup system.
 * Provides structured logging with file and console output.
 */
class Logger {
public:
    /**
     * Initialize the logger with the given name and optional file output.
     * @param name Logger name (e.g., "server" or "client")
     * @param logToFile Whether to also log to file
     * @param logDir Directory for log files
     */
    static void init(const std::string& name, bool logToFile = false, 
                     const std::string& logDir = "logs");

    /**
     * Get the logger instance.
     * @return Shared pointer to the logger
     */
    static std::shared_ptr<spdlog::logger> get();

    /**
     * Set the log level.
     * @param level Log level (trace, debug, info, warn, error, critical)
     */
    static void setLevel(spdlog::level::level_enum level);

    /**
     * Flush all log sinks.
     */
    static void flush();

private:
    static std::shared_ptr<spdlog::logger> logger_;
    static bool initialized_;
};

// Convenience macros for logging
#define LOG_TRACE(...) SPDLOG_LOGGER_TRACE(backup::Logger::get(), __VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_LOGGER_DEBUG(backup::Logger::get(), __VA_ARGS__)
#define LOG_INFO(...) SPDLOG_LOGGER_INFO(backup::Logger::get(), __VA_ARGS__)
#define LOG_WARN(...) SPDLOG_LOGGER_WARN(backup::Logger::get(), __VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_LOGGER_ERROR(backup::Logger::get(), __VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(backup::Logger::get(), __VA_ARGS__)

// Structured logging helpers
#define LOG_CMD(cmd, filename, error, latency_ms) \
    LOG_INFO("cmd={} filename={} error={} latency_ms={}", cmd, filename, error, latency_ms)

#define LOG_TRANSFER(filename, bytes, direction, latency_ms) \
    LOG_INFO("transfer filename={} bytes={} direction={} latency_ms={}", \
             filename, bytes, direction, latency_ms)

}  // namespace backup

#endif  // BACKUP_COMMON_LOGGER_H
