#ifndef BACKUP_COMMON_ERROR_CODES_H
#define BACKUP_COMMON_ERROR_CODES_H

#include <cstdint>
#include <string>

namespace backup {

// Error codes for the backup system
enum class ErrorCode : uint16_t {
    // Success
    SUCCESS = 0,

    // General errors (1-99)
    UNKNOWN_ERROR = 1,
    INVALID_ARGUMENT = 2,
    TIMEOUT = 3,
    CANCELLED = 4,

    // File errors (100-199)
    FILE_NOT_FOUND = 100,
    FILE_EXISTS = 101,
    FILE_OPEN_ERROR = 102,
    FILE_READ_ERROR = 103,
    FILE_WRITE_ERROR = 104,
    FILE_DELETE_ERROR = 105,
    FILE_RENAME_ERROR = 106,
    INVALID_FILENAME = 107,
    PATH_TRAVERSAL_ATTEMPT = 108,
    FILE_TOO_LARGE = 109,

    // Directory errors (200-299)
    DIRECTORY_NOT_FOUND = 200,
    DIRECTORY_CREATE_ERROR = 201,
    DIRECTORY_READ_ERROR = 202,

    // Network errors (300-399)
    SOCKET_CREATE_ERROR = 300,
    SOCKET_BIND_ERROR = 301,
    SOCKET_LISTEN_ERROR = 302,
    SOCKET_ACCEPT_ERROR = 303,
    SOCKET_CONNECT_ERROR = 304,
    SOCKET_SEND_ERROR = 305,
    SOCKET_RECV_ERROR = 306,
    CONNECTION_CLOSED = 307,
    CONNECTION_TIMEOUT = 308,

    // Protocol errors (400-499)
    INVALID_COMMAND = 400,
    INVALID_MESSAGE = 401,
    UNEXPECTED_RESPONSE = 402,
    PROTOCOL_ERROR = 403,

    // Server errors (500-599)
    SERVER_BUSY = 500,
    SERVER_SHUTDOWN = 501,
};

// Convert error code to string
const char* errorCodeToString(ErrorCode code);

// Convert error code to uint16_t for network transmission
inline uint16_t toNetworkError(ErrorCode code) {
    return static_cast<uint16_t>(code);
}

// Convert network error to ErrorCode
inline ErrorCode fromNetworkError(uint16_t error) {
    return static_cast<ErrorCode>(error);
}

// Check if error code indicates success
inline bool isSuccess(ErrorCode code) {
    return code == ErrorCode::SUCCESS;
}

// Error code description
std::string getErrorDescription(ErrorCode code);

}  // namespace backup

#endif  // BACKUP_COMMON_ERROR_CODES_H
