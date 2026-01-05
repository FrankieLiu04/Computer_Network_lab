#ifndef BACKUP_SERVER_SERVER_H
#define BACKUP_SERVER_SERVER_H

#include "protocol/message.h"
#include "common/error_codes.h"
#include <string>
#include <atomic>
#include <functional>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_t;
#define INVALID_SOCKET_VALUE INVALID_SOCKET
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int socket_t;
#define INVALID_SOCKET_VALUE (-1)
#define closesocket close
#endif

namespace backup {
namespace server {

// Server state enumeration
enum class ServerState {
    WAITING,
    PROCESS_LS,
    PROCESS_SEND,
    PROCESS_GET,
    PROCESS_REMOVE,
    PROCESS_RENAME,
    SHUTDOWN
};

// Server configuration
struct ServerConfig {
    uint16_t udpPort = 0;           // UDP command port (0 = auto assign)
    uint16_t tcpPort = 40000;       // Base TCP port for file transfers
    std::string backupDir = "backup";  // Backup directory
    int recvTimeoutMs = 5000;       // Receive timeout in milliseconds
    size_t maxFileSize = 100 * 1024 * 1024;  // 100 MB max file size
    bool enableHandshake = true;    // Enable explicit handshake for rename
};

// Forward declaration
class CommandHandler;

/**
 * Backup Server class.
 * Handles UDP commands and TCP file transfers.
 */
class Server {
public:
    explicit Server(const ServerConfig& config = ServerConfig());
    ~Server();

    // Disable copy
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    /**
     * Initialize the server (create sockets, bind ports).
     * @return ErrorCode indicating success or failure
     */
    ErrorCode initialize();

    /**
     * Run the server main loop.
     * Blocks until shutdown is requested.
     * @return ErrorCode indicating how the server exited
     */
    ErrorCode run();

    /**
     * Request the server to shutdown.
     * Can be called from another thread.
     */
    void requestShutdown();

    /**
     * Check if the server is running.
     * @return true if running, false otherwise
     */
    bool isRunning() const { return running_.load(); }

    /**
     * Get the actual UDP port the server is listening on.
     * @return UDP port number
     */
    uint16_t getUdpPort() const { return actualUdpPort_; }

    /**
     * Get the current TCP port for file transfers.
     * @return TCP port number
     */
    uint16_t getCurrentTcpPort() const { return currentTcpPort_; }

private:
    // Process incoming UDP command
    ErrorCode processCommand(const protocol::CmdMsg& cmd, 
                            const struct sockaddr_in& clientAddr);

    // Command handlers
    ErrorCode handleLs(const struct sockaddr_in& clientAddr);
    ErrorCode handleSend(const protocol::CmdMsg& cmd, 
                        const struct sockaddr_in& clientAddr);
    ErrorCode handleRemove(const protocol::CmdMsg& cmd,
                          const struct sockaddr_in& clientAddr);
    ErrorCode handleRename(const protocol::CmdMsg& cmd,
                          const struct sockaddr_in& clientAddr);
    ErrorCode handleShutdown(const struct sockaddr_in& clientAddr);

    // File transfer helpers
    ErrorCode receiveSmallFile(const std::string& filepath, size_t filesize,
                              const struct sockaddr_in& clientAddr);
    ErrorCode receiveLargeFile(const std::string& filepath, size_t filesize,
                              const struct sockaddr_in& clientAddr);

    // Send response to client
    ErrorCode sendResponse(const protocol::CmdMsg& response,
                          const struct sockaddr_in& clientAddr);
    ErrorCode sendDataResponse(const protocol::DataMsg& data,
                              const struct sockaddr_in& clientAddr);

    // Print banner
    void printBanner() const;
    void printWaiting() const;

    // Configuration
    ServerConfig config_;
    
    // Socket handles
    socket_t udpSocket_ = INVALID_SOCKET_VALUE;
    uint16_t actualUdpPort_ = 0;
    uint16_t currentTcpPort_ = 0;
    
    // Client address for current session
    struct sockaddr_in currentClientAddr_;
    socklen_t clientAddrLen_ = sizeof(struct sockaddr_in);

    // State management
    std::atomic<bool> running_{false};
    std::atomic<bool> shutdownRequested_{false};
    ServerState state_ = ServerState::WAITING;

#ifdef _WIN32
    bool wsaInitialized_ = false;
#endif
};

}  // namespace server
}  // namespace backup

#endif  // BACKUP_SERVER_SERVER_H
