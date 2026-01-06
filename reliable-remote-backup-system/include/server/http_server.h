#ifndef BACKUP_SERVER_HTTP_SERVER_H
#define BACKUP_SERVER_HTTP_SERVER_H

#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <functional>

// Forward declaration for httplib
namespace httplib {
    class Server;
}

namespace backup {
namespace server {

// HTTP server configuration
struct HttpServerConfig {
    uint16_t port = 8080;              // HTTP port
    std::string host = "0.0.0.0";      // Bind address
    std::string backupDir = "backup";  // Backup directory path
    std::string staticDir = "static";  // Static files directory
    size_t maxUploadSize = 100 * 1024 * 1024;  // 100 MB max upload
};

/**
 * HTTP Server for Web management console and metrics API.
 * Provides REST API endpoints for file management and observability.
 */
class HttpServer {
public:
    explicit HttpServer(const HttpServerConfig& config = HttpServerConfig());
    ~HttpServer();

    // Disable copy
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    /**
     * Start the HTTP server in a background thread.
     * @return true if started successfully
     */
    bool start();

    /**
     * Stop the HTTP server.
     */
    void stop();

    /**
     * Check if the server is running.
     */
    bool isRunning() const { return running_.load(); }

    /**
     * Get the port the server is listening on.
     */
    uint16_t getPort() const { return config_.port; }

    /**
     * Set callback for file operations (to coordinate with main server)
     */
    using FileListCallback = std::function<std::vector<std::string>()>;
    using FileDeleteCallback = std::function<bool(const std::string&)>;
    using FileRenameCallback = std::function<bool(const std::string&, const std::string&)>;

    void setFileListCallback(FileListCallback callback) { fileListCallback_ = callback; }
    void setFileDeleteCallback(FileDeleteCallback callback) { fileDeleteCallback_ = callback; }
    void setFileRenameCallback(FileRenameCallback callback) { fileRenameCallback_ = callback; }

private:
    // Setup routes
    void setupRoutes();

    // API handlers
    void handleHealthz();
    void handleGetFiles();
    void handleUploadFile();
    void handleDeleteFile();
    void handleRenameFile();
    void handleGetMetrics();

    // Configuration
    HttpServerConfig config_;

    // HTTP server instance
    std::unique_ptr<httplib::Server> server_;

    // Background thread
    std::thread serverThread_;
    std::atomic<bool> running_{false};

    // Callbacks for file operations
    FileListCallback fileListCallback_;
    FileDeleteCallback fileDeleteCallback_;
    FileRenameCallback fileRenameCallback_;
};

}  // namespace server
}  // namespace backup

#endif  // BACKUP_SERVER_HTTP_SERVER_H
