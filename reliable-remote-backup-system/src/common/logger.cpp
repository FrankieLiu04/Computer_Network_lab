#include "common/logger.h"
#include <filesystem>

namespace backup {

std::shared_ptr<spdlog::logger> Logger::logger_ = nullptr;
bool Logger::initialized_ = false;

void Logger::init(const std::string& name, bool logToFile, const std::string& logDir) {
    if (initialized_) {
        return;
    }

    std::vector<spdlog::sink_ptr> sinks;

    // Console sink with colors
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(spdlog::level::trace);
    consoleSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");
    sinks.push_back(consoleSink);

    // File sink (optional)
    if (logToFile) {
        try {
            // Create log directory if it doesn't exist
            std::filesystem::create_directories(logDir);

            std::string logFile = logDir + "/" + name + ".log";
            auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                logFile, 
                5 * 1024 * 1024,  // 5 MB max size
                3                  // 3 backup files
            );
            fileSink->set_level(spdlog::level::trace);
            fileSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] [%t] %v");
            sinks.push_back(fileSink);
        } catch (const spdlog::spdlog_ex& ex) {
            // If file logging fails, continue with console only
            fprintf(stderr, "Warning: Failed to create log file: %s\n", ex.what());
        }
    }

    // Create the logger
    logger_ = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
    logger_->set_level(spdlog::level::debug);
    logger_->flush_on(spdlog::level::warn);

    // Register the logger
    spdlog::register_logger(logger_);

    initialized_ = true;
}

std::shared_ptr<spdlog::logger> Logger::get() {
    if (!initialized_) {
        // Initialize with default settings if not already done
        init("backup", false);
    }
    return logger_;
}

void Logger::setLevel(spdlog::level::level_enum level) {
    if (logger_) {
        logger_->set_level(level);
    }
}

void Logger::flush() {
    if (logger_) {
        logger_->flush();
    }
}

}  // namespace backup
