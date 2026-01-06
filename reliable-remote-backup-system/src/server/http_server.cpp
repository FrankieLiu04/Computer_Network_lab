#include "server/http_server.h"
#include "common/metrics.h"
#include "common/file_utils.h"
#include "common/logger.h"
#include "common/utils.h"

#define CPPHTTPLIB_OPENSSL_SUPPORT 0
#include <httplib.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
#include <filesystem>

namespace backup {
namespace server {

using json = nlohmann::json;
namespace fs = std::filesystem;

HttpServer::HttpServer(const HttpServerConfig& config)
    : config_(config), server_(std::make_unique<httplib::Server>()) {
}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::start() {
    if (running_.load()) {
        LOG_WARN("HTTP server is already running");
        return false;
    }

    // Setup routes
    setupRoutes();

    // Start server in background thread
    serverThread_ = std::thread([this]() {
        running_.store(true);
        LOG_INFO("HTTP server starting on {}:{}", config_.host, config_.port);
        
        if (!server_->listen(config_.host.c_str(), config_.port)) {
            LOG_ERROR("HTTP server failed to start on {}:{}", config_.host, config_.port);
        }
        
        running_.store(false);
        LOG_INFO("HTTP server stopped");
    });

    // Wait a bit for the server to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return running_.load();
}

void HttpServer::stop() {
    if (running_.load() && server_) {
        LOG_INFO("Stopping HTTP server...");
        server_->stop();
    }
    
    if (serverThread_.joinable()) {
        serverThread_.join();
    }
}

void HttpServer::setupRoutes() {
    // Health check endpoint
    server_->Get("/healthz", [](const httplib::Request& req, httplib::Response& res) {
        std::string reqId = utils::generateRequestId();
        Metrics::instance().recordHttpRequest("/healthz", "GET");
        json response = {
            {"status", "ok"},
            {"uptime_seconds", Metrics::instance().getUptimeSeconds()},
            {"request_id", reqId}
        };
        res.set_header("X-Request-ID", reqId);
        res.set_content(response.dump(), "application/json");
    });

    // Get file list
    server_->Get("/api/files", [this](const httplib::Request& req, httplib::Response& res) {
        std::string reqId = utils::generateRequestId();
        Metrics::instance().recordHttpRequest("/api/files", "GET");
        auto timer = Metrics::instance().startTimer("http_ls");
        
        LOG_INFO("[{}] GET /api/files", reqId);
        
        try {
            std::vector<std::string> files;
            
            // Use callback if set, otherwise read directly
            if (fileListCallback_) {
                files = fileListCallback_();
            } else {
                files = file::listDirectory(config_.backupDir);
            }
            
            json fileList = json::array();
            for (const auto& filename : files) {
                std::string fullPath = config_.backupDir + "/" + filename;
                json fileInfo = {
                    {"name", filename},
                    {"size", file::getFileSize(fullPath)}
                };
                fileList.push_back(fileInfo);
            }
            
            json response = {
                {"success", true},
                {"files", fileList},
                {"count", files.size()},
                {"request_id", reqId}
            };
            res.set_header("X-Request-ID", reqId);
            res.set_content(response.dump(), "application/json");
            
            LOG_INFO("[{}] GET /api/files completed: {} files", reqId, files.size());
            
        } catch (const std::exception& e) {
            LOG_ERROR("[{}] Error listing files: {}", reqId, e.what());
            json response = {
                {"success", false},
                {"error", e.what()},
                {"request_id", reqId}
            };
            res.set_header("X-Request-ID", reqId);
            res.status = 500;
            res.set_content(response.dump(), "application/json");
        }
    });

    // Upload file
    server_->Post("/api/upload", [this](const httplib::Request& req, httplib::Response& res) {
        std::string reqId = utils::generateRequestId();
        Metrics::instance().recordHttpRequest("/api/upload", "POST");
        auto timer = Metrics::instance().startTimer("http_upload");
        
        res.set_header("X-Request-ID", reqId);
        
        if (!req.has_file("file")) {
            LOG_WARN("[{}] Upload failed: no file provided", reqId);
            json response = {
                {"success", false},
                {"error", "No file provided"},
                {"request_id", reqId}
            };
            res.status = 400;
            res.set_content(response.dump(), "application/json");
            return;
        }
        
        const auto& file = req.get_file_value("file");
        LOG_INFO("[{}] Upload started: {} ({} bytes)", reqId, file.filename, file.content.size());
        
        // Validate filename
        if (!utils::isValidFilename(file.filename)) {
            LOG_WARN("[{}] Upload failed: invalid filename '{}'", reqId, file.filename);
            json response = {
                {"success", false},
                {"error", "Invalid filename"},
                {"request_id", reqId}
            };
            res.status = 400;
            res.set_content(response.dump(), "application/json");
            Metrics::instance().recordError("http_upload", "invalid_filename");
            return;
        }
        
        // Check file size
        if (file.content.size() > config_.maxUploadSize) {
            LOG_WARN("[{}] Upload failed: file too large ({} > {})", 
                     reqId, file.content.size(), config_.maxUploadSize);
            json response = {
                {"success", false},
                {"error", "File too large"},
                {"request_id", reqId}
            };
            res.status = 413;
            res.set_content(response.dump(), "application/json");
            Metrics::instance().recordError("http_upload", "file_too_large");
            return;
        }
        
        try {
            // Ensure backup directory exists
            file::createDirectory(config_.backupDir);
            
            std::string filepath = config_.backupDir + "/" + file.filename;
            
            // Write file
            std::ofstream ofs(filepath, std::ios::binary);
            if (!ofs) {
                throw std::runtime_error("Failed to create file");
            }
            ofs.write(file.content.data(), file.content.size());
            ofs.close();
            
            Metrics::instance().recordBytesReceived(file.content.size());
            Metrics::instance().recordSendSession();
            Metrics::instance().recordRequest("http_upload");
            
            json response = {
                {"success", true},
                {"filename", file.filename},
                {"size", file.content.size()},
                {"request_id", reqId}
            };
            res.set_content(response.dump(), "application/json");
            LOG_INFO("[{}] Upload completed: {} ({} bytes)", reqId, file.filename, file.content.size());
            
        } catch (const std::exception& e) {
            LOG_ERROR("[{}] Upload error: {}", reqId, e.what());
            Metrics::instance().recordSendFailure();
            Metrics::instance().recordError("http_upload", "write_error");
            json response = {
                {"success", false},
                {"error", e.what()},
                {"request_id", reqId}
            };
            res.status = 500;
            res.set_content(response.dump(), "application/json");
        }
    });

    // Delete file
    server_->Delete(R"(/api/files/(.+))", [this](const httplib::Request& req, httplib::Response& res) {
        std::string reqId = utils::generateRequestId();
        Metrics::instance().recordHttpRequest("/api/files", "DELETE");
        auto timer = Metrics::instance().startTimer("http_delete");
        
        res.set_header("X-Request-ID", reqId);
        std::string filename = req.matches[1].str();
        
        LOG_INFO("[{}] DELETE /api/files/{}", reqId, filename);
        
        // Validate filename
        if (!utils::isValidFilename(filename)) {
            LOG_WARN("[{}] Delete failed: invalid filename '{}'", reqId, filename);
            json response = {
                {"success", false},
                {"error", "Invalid filename"},
                {"request_id", reqId}
            };
            res.status = 400;
            res.set_content(response.dump(), "application/json");
            Metrics::instance().recordError("http_delete", "invalid_filename");
            return;
        }
        
        try {
            std::string filepath = config_.backupDir + "/" + filename;
            
            // Check if file exists
            if (!file::fileExists(filepath)) {
                LOG_WARN("[{}] Delete failed: file not found '{}'", reqId, filename);
                json response = {
                    {"success", false},
                    {"error", "File not found"},
                    {"request_id", reqId}
                };
                res.status = 404;
                res.set_content(response.dump(), "application/json");
                Metrics::instance().recordError("http_delete", "not_found");
                return;
            }
            
            // Delete file
            bool success = false;
            if (fileDeleteCallback_) {
                success = fileDeleteCallback_(filename);
            } else {
                success = file::deleteFile(filepath);
            }
            
            if (success) {
                Metrics::instance().recordRequest("http_delete");
                LOG_INFO("[{}] Delete completed: {}", reqId, filename);
                json response = {
                    {"success", true},
                    {"filename", filename},
                    {"request_id", reqId}
                };
                res.set_content(response.dump(), "application/json");
            } else {
                LOG_ERROR("[{}] Delete failed: {}", reqId, filename);
                Metrics::instance().recordError("http_delete", "delete_failed");
                json response = {
                    {"success", false},
                    {"error", "Failed to delete file"},
                    {"request_id", reqId}
                };
                res.status = 500;
                res.set_content(response.dump(), "application/json");
            }
            
        } catch (const std::exception& e) {
            LOG_ERROR("[{}] Delete error: {}", reqId, e.what());
            Metrics::instance().recordError("http_delete", "exception");
            json response = {
                {"success", false},
                {"error", e.what()},
                {"request_id", reqId}
            };
            res.status = 500;
            res.set_content(response.dump(), "application/json");
        }
    });

    // Rename file
    server_->Post("/api/rename", [this](const httplib::Request& req, httplib::Response& res) {
        std::string reqId = utils::generateRequestId();
        Metrics::instance().recordHttpRequest("/api/rename", "POST");
        auto timer = Metrics::instance().startTimer("http_rename");
        
        res.set_header("X-Request-ID", reqId);
        
        try {
            json body = json::parse(req.body);
            
            if (!body.contains("oldName") || !body.contains("newName")) {
                LOG_WARN("[{}] Rename failed: missing parameters", reqId);
                json response = {
                    {"success", false},
                    {"error", "Missing oldName or newName"},
                    {"request_id", reqId}
                };
                res.status = 400;
                res.set_content(response.dump(), "application/json");
                return;
            }
            
            std::string oldName = body["oldName"].get<std::string>();
            std::string newName = body["newName"].get<std::string>();
            
            LOG_INFO("[{}] Rename: {} -> {}", reqId, oldName, newName);
            
            // Validate filenames
            if (!utils::isValidFilename(oldName) || !utils::isValidFilename(newName)) {
                LOG_WARN("[{}] Rename failed: invalid filename", reqId);
                json response = {
                    {"success", false},
                    {"error", "Invalid filename"},
                    {"request_id", reqId}
                };
                res.status = 400;
                res.set_content(response.dump(), "application/json");
                Metrics::instance().recordError("http_rename", "invalid_filename");
                return;
            }
            
            std::string oldPath = config_.backupDir + "/" + oldName;
            std::string newPath = config_.backupDir + "/" + newName;
            
            // Check if source exists
            if (!file::fileExists(oldPath)) {
                LOG_WARN("[{}] Rename failed: source not found '{}'", reqId, oldName);
                json response = {
                    {"success", false},
                    {"error", "Source file not found"},
                    {"request_id", reqId}
                };
                res.status = 404;
                res.set_content(response.dump(), "application/json");
                Metrics::instance().recordError("http_rename", "not_found");
                return;
            }
            
            // Check if target exists
            if (file::fileExists(newPath)) {
                LOG_WARN("[{}] Rename failed: target exists '{}'", reqId, newName);
                json response = {
                    {"success", false},
                    {"error", "Target file already exists"},
                    {"request_id", reqId}
                };
                res.status = 409;
                res.set_content(response.dump(), "application/json");
                Metrics::instance().recordError("http_rename", "exists");
                return;
            }
            
            // Rename file
            bool success = false;
            if (fileRenameCallback_) {
                success = fileRenameCallback_(oldName, newName);
            } else {
                success = file::renameFile(oldPath, newPath);
            }
            
            if (success) {
                Metrics::instance().recordRequest("http_rename");
                LOG_INFO("[{}] Rename completed: {} -> {}", reqId, oldName, newName);
                json response = {
                    {"success", true},
                    {"oldName", oldName},
                    {"newName", newName},
                    {"request_id", reqId}
                };
                res.set_content(response.dump(), "application/json");
            } else {
                LOG_ERROR("[{}] Rename failed: {} -> {}", reqId, oldName, newName);
                Metrics::instance().recordError("http_rename", "rename_failed");
                json response = {
                    {"success", false},
                    {"error", "Failed to rename file"},
                    {"request_id", reqId}
                };
                res.status = 500;
                res.set_content(response.dump(), "application/json");
            }
            
        } catch (const json::exception& e) {
            LOG_ERROR("[{}] JSON parse error: {}", reqId, e.what());
            json response = {
                {"success", false},
                {"error", "Invalid JSON"},
                {"request_id", reqId}
            };
            res.status = 400;
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            LOG_ERROR("[{}] Rename error: {}", reqId, e.what());
            Metrics::instance().recordError("http_rename", "exception");
            json response = {
                {"success", false},
                {"error", e.what()},
                {"request_id", reqId}
            };
            res.status = 500;
            res.set_content(response.dump(), "application/json");
        }
    });

    // Get metrics
    server_->Get("/api/metrics", [](const httplib::Request& req, httplib::Response& res) {
        std::string reqId = utils::generateRequestId();
        Metrics::instance().recordHttpRequest("/api/metrics", "GET");
        
        res.set_header("X-Request-ID", reqId);
        auto& metrics = Metrics::instance();
        
        // Get all error counts for detailed breakdown
        auto allErrors = metrics.getAllErrorCounts();
        json errorDetails = json::object();
        for (const auto& [key, count] : allErrors) {
            errorDetails[key] = count;
        }
        
        json response = {
            {"uptime_seconds", metrics.getUptimeSeconds()},
            {"start_time", metrics.getStartTime()},
            {"request_id", reqId},
            {"requests", {
                {"by_command", {
                    {"ls", metrics.getRequestCount("ls")},
                    {"send", metrics.getRequestCount("send")},
                    {"remove", metrics.getRequestCount("remove")},
                    {"rename", metrics.getRequestCount("rename")}
                }},
                {"by_http_endpoint", {
                    {"upload", metrics.getRequestCount("http_upload")},
                    {"delete", metrics.getRequestCount("http_delete")},
                    {"rename", metrics.getRequestCount("http_rename")}
                }},
                {"http_total", metrics.getHttpRequestCount()}
            }},
            {"errors", {
                {"by_command", {
                    {"ls", metrics.getErrorCount("ls")},
                    {"send", metrics.getErrorCount("send")},
                    {"remove", metrics.getErrorCount("remove")},
                    {"rename", metrics.getErrorCount("rename")}
                }},
                {"by_http_endpoint", {
                    {"upload", metrics.getErrorCount("http_upload")},
                    {"delete", metrics.getErrorCount("http_delete")},
                    {"rename", metrics.getErrorCount("http_rename")}
                }},
                {"details", errorDetails}
            }},
            {"transfer", {
                {"bytes_received", metrics.getBytesReceived()},
                {"bytes_sent", metrics.getBytesSent()},
                {"send_sessions", metrics.getSendSessions()},
                {"send_failures", metrics.getSendFailures()},
                {"success_rate_pct", metrics.getSendSessions() > 0 
                    ? 100.0 * (metrics.getSendSessions() - metrics.getSendFailures()) / metrics.getSendSessions()
                    : 100.0}
            }},
            {"latency", {
                {"ls", {
                    {"avg_ms", metrics.getAverageLatency("ls")},
                    {"max_ms", metrics.getMaxLatency("ls")},
                    {"p50_ms", metrics.getLatencyPercentile("ls", 50)},
                    {"p95_ms", metrics.getLatencyPercentile("ls", 95)},
                    {"p99_ms", metrics.getLatencyPercentile("ls", 99)}
                }},
                {"send", {
                    {"avg_ms", metrics.getAverageLatency("send")},
                    {"max_ms", metrics.getMaxLatency("send")},
                    {"p50_ms", metrics.getLatencyPercentile("send", 50)},
                    {"p95_ms", metrics.getLatencyPercentile("send", 95)},
                    {"p99_ms", metrics.getLatencyPercentile("send", 99)}
                }},
                {"http_upload", {
                    {"avg_ms", metrics.getAverageLatency("http_upload")},
                    {"max_ms", metrics.getMaxLatency("http_upload")},
                    {"p50_ms", metrics.getLatencyPercentile("http_upload", 50)},
                    {"p95_ms", metrics.getLatencyPercentile("http_upload", 95)},
                    {"p99_ms", metrics.getLatencyPercentile("http_upload", 99)}
                }},
                {"http_delete", {
                    {"avg_ms", metrics.getAverageLatency("http_delete")},
                    {"max_ms", metrics.getMaxLatency("http_delete")},
                    {"p50_ms", metrics.getLatencyPercentile("http_delete", 50)},
                    {"p95_ms", metrics.getLatencyPercentile("http_delete", 95)},
                    {"p99_ms", metrics.getLatencyPercentile("http_delete", 99)}
                }},
                {"http_rename", {
                    {"avg_ms", metrics.getAverageLatency("http_rename")},
                    {"max_ms", metrics.getMaxLatency("http_rename")},
                    {"p50_ms", metrics.getLatencyPercentile("http_rename", 50)},
                    {"p95_ms", metrics.getLatencyPercentile("http_rename", 95)},
                    {"p99_ms", metrics.getLatencyPercentile("http_rename", 99)}
                }}
            }}
        };
        
        res.set_content(response.dump(2), "application/json");
    });

    // Serve static files
    server_->set_mount_point("/", config_.staticDir);

    // CORS headers for development
    server_->set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"}
    });

    // Handle OPTIONS preflight requests
    server_->Options(".*", [](const httplib::Request& req, httplib::Response& res) {
        res.status = 204;
    });

    // Set payload max size
    server_->set_payload_max_length(config_.maxUploadSize);

    LOG_INFO("HTTP routes configured");
}

}  // namespace server
}  // namespace backup
