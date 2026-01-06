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
        Metrics::instance().recordHttpRequest("/healthz", "GET");
        json response = {
            {"status", "ok"},
            {"uptime_seconds", Metrics::instance().getUptimeSeconds()}
        };
        res.set_content(response.dump(), "application/json");
    });

    // Get file list
    server_->Get("/api/files", [this](const httplib::Request& req, httplib::Response& res) {
        Metrics::instance().recordHttpRequest("/api/files", "GET");
        auto timer = Metrics::instance().startTimer("http_ls");
        
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
                {"count", files.size()}
            };
            res.set_content(response.dump(), "application/json");
            
        } catch (const std::exception& e) {
            LOG_ERROR("Error listing files: {}", e.what());
            json response = {
                {"success", false},
                {"error", e.what()}
            };
            res.status = 500;
            res.set_content(response.dump(), "application/json");
        }
    });

    // Upload file
    server_->Post("/api/upload", [this](const httplib::Request& req, httplib::Response& res) {
        Metrics::instance().recordHttpRequest("/api/upload", "POST");
        auto timer = Metrics::instance().startTimer("http_upload");
        
        if (!req.has_file("file")) {
            json response = {
                {"success", false},
                {"error", "No file provided"}
            };
            res.status = 400;
            res.set_content(response.dump(), "application/json");
            return;
        }
        
        const auto& file = req.get_file_value("file");
        
        // Validate filename
        if (!utils::isValidFilename(file.filename)) {
            json response = {
                {"success", false},
                {"error", "Invalid filename"}
            };
            res.status = 400;
            res.set_content(response.dump(), "application/json");
            Metrics::instance().recordError("http_upload", "invalid_filename");
            return;
        }
        
        // Check file size
        if (file.content.size() > config_.maxUploadSize) {
            json response = {
                {"success", false},
                {"error", "File too large"}
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
                {"size", file.content.size()}
            };
            res.set_content(response.dump(), "application/json");
            LOG_INFO("File uploaded via HTTP: {} ({} bytes)", file.filename, file.content.size());
            
        } catch (const std::exception& e) {
            LOG_ERROR("Error uploading file: {}", e.what());
            Metrics::instance().recordSendFailure();
            Metrics::instance().recordError("http_upload", "write_error");
            json response = {
                {"success", false},
                {"error", e.what()}
            };
            res.status = 500;
            res.set_content(response.dump(), "application/json");
        }
    });

    // Delete file
    server_->Delete(R"(/api/files/(.+))", [this](const httplib::Request& req, httplib::Response& res) {
        Metrics::instance().recordHttpRequest("/api/files", "DELETE");
        auto timer = Metrics::instance().startTimer("http_delete");
        
        std::string filename = req.matches[1].str();
        
        // Validate filename
        if (!utils::isValidFilename(filename)) {
            json response = {
                {"success", false},
                {"error", "Invalid filename"}
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
                json response = {
                    {"success", false},
                    {"error", "File not found"}
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
                json response = {
                    {"success", true},
                    {"filename", filename}
                };
                res.set_content(response.dump(), "application/json");
                LOG_INFO("File deleted via HTTP: {}", filename);
            } else {
                Metrics::instance().recordError("http_delete", "delete_failed");
                json response = {
                    {"success", false},
                    {"error", "Failed to delete file"}
                };
                res.status = 500;
                res.set_content(response.dump(), "application/json");
            }
            
        } catch (const std::exception& e) {
            LOG_ERROR("Error deleting file: {}", e.what());
            Metrics::instance().recordError("http_delete", "exception");
            json response = {
                {"success", false},
                {"error", e.what()}
            };
            res.status = 500;
            res.set_content(response.dump(), "application/json");
        }
    });

    // Rename file
    server_->Post("/api/rename", [this](const httplib::Request& req, httplib::Response& res) {
        Metrics::instance().recordHttpRequest("/api/rename", "POST");
        auto timer = Metrics::instance().startTimer("http_rename");
        
        try {
            json body = json::parse(req.body);
            
            if (!body.contains("oldName") || !body.contains("newName")) {
                json response = {
                    {"success", false},
                    {"error", "Missing oldName or newName"}
                };
                res.status = 400;
                res.set_content(response.dump(), "application/json");
                return;
            }
            
            std::string oldName = body["oldName"].get<std::string>();
            std::string newName = body["newName"].get<std::string>();
            
            // Validate filenames
            if (!utils::isValidFilename(oldName) || !utils::isValidFilename(newName)) {
                json response = {
                    {"success", false},
                    {"error", "Invalid filename"}
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
                json response = {
                    {"success", false},
                    {"error", "Source file not found"}
                };
                res.status = 404;
                res.set_content(response.dump(), "application/json");
                Metrics::instance().recordError("http_rename", "not_found");
                return;
            }
            
            // Check if target exists
            if (file::fileExists(newPath)) {
                json response = {
                    {"success", false},
                    {"error", "Target file already exists"}
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
                json response = {
                    {"success", true},
                    {"oldName", oldName},
                    {"newName", newName}
                };
                res.set_content(response.dump(), "application/json");
                LOG_INFO("File renamed via HTTP: {} -> {}", oldName, newName);
            } else {
                Metrics::instance().recordError("http_rename", "rename_failed");
                json response = {
                    {"success", false},
                    {"error", "Failed to rename file"}
                };
                res.status = 500;
                res.set_content(response.dump(), "application/json");
            }
            
        } catch (const json::exception& e) {
            LOG_ERROR("JSON parse error: {}", e.what());
            json response = {
                {"success", false},
                {"error", "Invalid JSON"}
            };
            res.status = 400;
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            LOG_ERROR("Error renaming file: {}", e.what());
            Metrics::instance().recordError("http_rename", "exception");
            json response = {
                {"success", false},
                {"error", e.what()}
            };
            res.status = 500;
            res.set_content(response.dump(), "application/json");
        }
    });

    // Get metrics
    server_->Get("/api/metrics", [](const httplib::Request& req, httplib::Response& res) {
        Metrics::instance().recordHttpRequest("/api/metrics", "GET");
        
        auto& metrics = Metrics::instance();
        
        json response = {
            {"uptime_seconds", metrics.getUptimeSeconds()},
            {"start_time", metrics.getStartTime()},
            {"requests", {
                {"total", {
                    {"ls", metrics.getRequestCount("ls")},
                    {"send", metrics.getRequestCount("send")},
                    {"remove", metrics.getRequestCount("remove")},
                    {"rename", metrics.getRequestCount("rename")},
                    {"http_upload", metrics.getRequestCount("http_upload")},
                    {"http_delete", metrics.getRequestCount("http_delete")},
                    {"http_rename", metrics.getRequestCount("http_rename")}
                }},
                {"http_total", metrics.getHttpRequestCount()}
            }},
            {"errors", {
                {"total", {
                    {"ls", metrics.getErrorCount("ls")},
                    {"send", metrics.getErrorCount("send")},
                    {"remove", metrics.getErrorCount("remove")},
                    {"rename", metrics.getErrorCount("rename")}
                }}
            }},
            {"transfer", {
                {"bytes_received", metrics.getBytesReceived()},
                {"bytes_sent", metrics.getBytesSent()},
                {"send_sessions", metrics.getSendSessions()},
                {"send_failures", metrics.getSendFailures()}
            }},
            {"latency", {
                {"ls", {
                    {"avg_ms", metrics.getAverageLatency("ls")},
                    {"max_ms", metrics.getMaxLatency("ls")},
                    {"p95_ms", metrics.getLatencyPercentile("ls", 95)}
                }},
                {"send", {
                    {"avg_ms", metrics.getAverageLatency("send")},
                    {"max_ms", metrics.getMaxLatency("send")},
                    {"p95_ms", metrics.getLatencyPercentile("send", 95)}
                }},
                {"http_upload", {
                    {"avg_ms", metrics.getAverageLatency("http_upload")},
                    {"max_ms", metrics.getMaxLatency("http_upload")},
                    {"p95_ms", metrics.getLatencyPercentile("http_upload", 95)}
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
