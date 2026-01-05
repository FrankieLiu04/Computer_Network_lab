#ifndef BACKUP_CLIENT_CLIENT_H
#define BACKUP_CLIENT_CLIENT_H

#include "protocol/message.h"
#include "common/error_codes.h"
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_t;
#define INVALID_SOCKET_VALUE INVALID_SOCKET
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
typedef int socket_t;
#define INVALID_SOCKET_VALUE (-1)
#define closesocket close
#endif

namespace backup {
namespace client {

// Client state enumeration
enum class ClientState {
    WAITING,
    PROCESS_LS,
    PROCESS_SEND,
    PROCESS_GET,
    PROCESS_REMOVE,
    PROCESS_RENAME,
    SHUTDOWN,
    QUIT
};

// Client configuration
struct ClientConfig {
    std::string serverHost = "127.0.0.1";
    uint16_t serverPort = 0;            // Required: server UDP port
    int recvTimeoutMs = 5000;           // Receive timeout in milliseconds
    bool enableHandshake = true;        // Enable explicit handshake for rename
};

/**
 * Backup Client class.
 * Handles user commands and communicates with the backup server.
 */
class Client {
public:
    explicit Client(const ClientConfig& config);
    ~Client();

    // Disable copy
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    /**
     * Initialize the client (create socket, resolve server address).
     * @return ErrorCode indicating success or failure
     */
    ErrorCode initialize();

    /**
     * Run the client main loop.
     * Blocks until quit is requested.
     * @return ErrorCode indicating how the client exited
     */
    ErrorCode run();

    /**
     * Execute a single command.
     * @param command The command string (e.g., "ls", "send file.txt")
     * @return ErrorCode indicating success or failure
     */
    ErrorCode executeCommand(const std::string& command);

private:
    // Command handlers
    ErrorCode handleLs();
    ErrorCode handleSend(const std::string& filename);
    ErrorCode handleRemove(const std::string& filename);
    ErrorCode handleRename(const std::string& oldName, const std::string& newName);
    ErrorCode handleShutdown();

    // File transfer helpers
    ErrorCode sendSmallFile(const std::string& filepath, size_t filesize);
    ErrorCode sendLargeFile(const std::string& filepath, size_t filesize, uint16_t tcpPort);

    // Communication helpers
    ErrorCode sendCommand(const protocol::CmdMsg& cmd);
    ErrorCode sendData(const protocol::DataMsg& data);
    ErrorCode receiveResponse(protocol::CmdMsg& response);
    ErrorCode receiveData(protocol::DataMsg& data);

    // Parse command input
    struct ParsedCommand {
        std::string command;
        std::string arg1;
        std::string arg2;
    };
    ParsedCommand parseCommand(const std::string& input);

    // Configuration
    ClientConfig config_;

    // Socket
    socket_t udpSocket_ = INVALID_SOCKET_VALUE;
    struct sockaddr_in serverAddr_;

    // State
    ClientState state_ = ClientState::WAITING;
    bool running_ = false;

#ifdef _WIN32
    bool wsaInitialized_ = false;
#endif
};

}  // namespace client
}  // namespace backup

#endif  // BACKUP_CLIENT_CLIENT_H
