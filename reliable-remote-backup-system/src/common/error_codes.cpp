#include "common/error_codes.h"

namespace backup {

const char* errorCodeToString(ErrorCode code) {
    switch (code) {
        // Success
        case ErrorCode::SUCCESS: return "SUCCESS";

        // General errors
        case ErrorCode::UNKNOWN_ERROR: return "UNKNOWN_ERROR";
        case ErrorCode::INVALID_ARGUMENT: return "INVALID_ARGUMENT";
        case ErrorCode::TIMEOUT: return "TIMEOUT";
        case ErrorCode::CANCELLED: return "CANCELLED";

        // File errors
        case ErrorCode::FILE_NOT_FOUND: return "FILE_NOT_FOUND";
        case ErrorCode::FILE_EXISTS: return "FILE_EXISTS";
        case ErrorCode::FILE_OPEN_ERROR: return "FILE_OPEN_ERROR";
        case ErrorCode::FILE_READ_ERROR: return "FILE_READ_ERROR";
        case ErrorCode::FILE_WRITE_ERROR: return "FILE_WRITE_ERROR";
        case ErrorCode::FILE_DELETE_ERROR: return "FILE_DELETE_ERROR";
        case ErrorCode::FILE_RENAME_ERROR: return "FILE_RENAME_ERROR";
        case ErrorCode::INVALID_FILENAME: return "INVALID_FILENAME";
        case ErrorCode::PATH_TRAVERSAL_ATTEMPT: return "PATH_TRAVERSAL_ATTEMPT";
        case ErrorCode::FILE_TOO_LARGE: return "FILE_TOO_LARGE";

        // Directory errors
        case ErrorCode::DIRECTORY_NOT_FOUND: return "DIRECTORY_NOT_FOUND";
        case ErrorCode::DIRECTORY_CREATE_ERROR: return "DIRECTORY_CREATE_ERROR";
        case ErrorCode::DIRECTORY_READ_ERROR: return "DIRECTORY_READ_ERROR";

        // Network errors
        case ErrorCode::SOCKET_CREATE_ERROR: return "SOCKET_CREATE_ERROR";
        case ErrorCode::SOCKET_BIND_ERROR: return "SOCKET_BIND_ERROR";
        case ErrorCode::SOCKET_LISTEN_ERROR: return "SOCKET_LISTEN_ERROR";
        case ErrorCode::SOCKET_ACCEPT_ERROR: return "SOCKET_ACCEPT_ERROR";
        case ErrorCode::SOCKET_CONNECT_ERROR: return "SOCKET_CONNECT_ERROR";
        case ErrorCode::SOCKET_SEND_ERROR: return "SOCKET_SEND_ERROR";
        case ErrorCode::SOCKET_RECV_ERROR: return "SOCKET_RECV_ERROR";
        case ErrorCode::CONNECTION_CLOSED: return "CONNECTION_CLOSED";
        case ErrorCode::CONNECTION_TIMEOUT: return "CONNECTION_TIMEOUT";

        // Protocol errors
        case ErrorCode::INVALID_COMMAND: return "INVALID_COMMAND";
        case ErrorCode::INVALID_MESSAGE: return "INVALID_MESSAGE";
        case ErrorCode::UNEXPECTED_RESPONSE: return "UNEXPECTED_RESPONSE";
        case ErrorCode::PROTOCOL_ERROR: return "PROTOCOL_ERROR";

        // Server errors
        case ErrorCode::SERVER_BUSY: return "SERVER_BUSY";
        case ErrorCode::SERVER_SHUTDOWN: return "SERVER_SHUTDOWN";

        default: return "UNKNOWN";
    }
}

std::string getErrorDescription(ErrorCode code) {
    switch (code) {
        // Success
        case ErrorCode::SUCCESS: 
            return "Operation completed successfully";

        // General errors
        case ErrorCode::UNKNOWN_ERROR: 
            return "An unknown error occurred";
        case ErrorCode::INVALID_ARGUMENT: 
            return "Invalid argument provided";
        case ErrorCode::TIMEOUT: 
            return "Operation timed out";
        case ErrorCode::CANCELLED: 
            return "Operation was cancelled";

        // File errors
        case ErrorCode::FILE_NOT_FOUND: 
            return "File not found";
        case ErrorCode::FILE_EXISTS: 
            return "File already exists";
        case ErrorCode::FILE_OPEN_ERROR: 
            return "Failed to open file";
        case ErrorCode::FILE_READ_ERROR: 
            return "Failed to read file";
        case ErrorCode::FILE_WRITE_ERROR: 
            return "Failed to write file";
        case ErrorCode::FILE_DELETE_ERROR: 
            return "Failed to delete file";
        case ErrorCode::FILE_RENAME_ERROR: 
            return "Failed to rename file";
        case ErrorCode::INVALID_FILENAME: 
            return "Invalid filename";
        case ErrorCode::PATH_TRAVERSAL_ATTEMPT: 
            return "Path traversal attempt detected";
        case ErrorCode::FILE_TOO_LARGE: 
            return "File is too large";

        // Directory errors
        case ErrorCode::DIRECTORY_NOT_FOUND: 
            return "Directory not found";
        case ErrorCode::DIRECTORY_CREATE_ERROR: 
            return "Failed to create directory";
        case ErrorCode::DIRECTORY_READ_ERROR: 
            return "Failed to read directory";

        // Network errors
        case ErrorCode::SOCKET_CREATE_ERROR: 
            return "Failed to create socket";
        case ErrorCode::SOCKET_BIND_ERROR: 
            return "Failed to bind socket";
        case ErrorCode::SOCKET_LISTEN_ERROR: 
            return "Failed to listen on socket";
        case ErrorCode::SOCKET_ACCEPT_ERROR: 
            return "Failed to accept connection";
        case ErrorCode::SOCKET_CONNECT_ERROR: 
            return "Failed to connect to server";
        case ErrorCode::SOCKET_SEND_ERROR: 
            return "Failed to send data";
        case ErrorCode::SOCKET_RECV_ERROR: 
            return "Failed to receive data";
        case ErrorCode::CONNECTION_CLOSED: 
            return "Connection closed by peer";
        case ErrorCode::CONNECTION_TIMEOUT: 
            return "Connection timed out";

        // Protocol errors
        case ErrorCode::INVALID_COMMAND: 
            return "Invalid command received";
        case ErrorCode::INVALID_MESSAGE: 
            return "Invalid message format";
        case ErrorCode::UNEXPECTED_RESPONSE: 
            return "Unexpected response from server";
        case ErrorCode::PROTOCOL_ERROR: 
            return "Protocol error";

        // Server errors
        case ErrorCode::SERVER_BUSY: 
            return "Server is busy";
        case ErrorCode::SERVER_SHUTDOWN: 
            return "Server is shutting down";

        default: 
            return "Unknown error code: " + std::to_string(static_cast<int>(code));
    }
}

}  // namespace backup
