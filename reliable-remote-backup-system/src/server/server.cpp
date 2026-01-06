#ifdef _WIN32
#define NOMINMAX  // Prevent Windows.h from defining min/max macros
#endif

#include "server/server.h"
#include "common/logger.h"
#include "common/utils.h"
#include "common/file_utils.h"
#include "common/metrics.h"

#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <limits>

#ifdef _WIN32
#include <fcntl.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <fcntl.h>
#include <sys/select.h>
#endif

namespace backup {
namespace server {

Server::Server(const ServerConfig& config) 
    : config_(config), currentTcpPort_(config.tcpPort) {
    std::memset(&currentClientAddr_, 0, sizeof(currentClientAddr_));
}

Server::~Server() {
    // Stop HTTP server first
    if (httpServer_) {
        httpServer_->stop();
    }

    if (udpSocket_ != INVALID_SOCKET_VALUE) {
        closesocket(udpSocket_);
    }

#ifdef _WIN32
    if (wsaInitialized_) {
        WSACleanup();
    }
#endif
}

ErrorCode Server::initialize() {
#ifdef _WIN32
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        LOG_ERROR("Failed to initialize Winsock");
        return ErrorCode::SOCKET_CREATE_ERROR;
    }
    wsaInitialized_ = true;
#endif

    // Create UDP socket
    udpSocket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSocket_ == INVALID_SOCKET_VALUE) {
        LOG_ERROR("Failed to create UDP socket");
        return ErrorCode::SOCKET_CREATE_ERROR;
    }

    // Set socket options
    int optval = 1;
    setsockopt(udpSocket_, SOL_SOCKET, SO_REUSEADDR, 
               reinterpret_cast<const char*>(&optval), sizeof(optval));

    // Bind to address
    struct sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(config_.udpPort);

    if (bind(udpSocket_, reinterpret_cast<struct sockaddr*>(&serverAddr), 
             sizeof(serverAddr)) < 0) {
        LOG_ERROR("Failed to bind UDP socket to port {}", config_.udpPort);
        return ErrorCode::SOCKET_BIND_ERROR;
    }

    // Get actual port if auto-assigned
    socklen_t addrLen = sizeof(serverAddr);
    getsockname(udpSocket_, reinterpret_cast<struct sockaddr*>(&serverAddr), &addrLen);
    actualUdpPort_ = ntohs(serverAddr.sin_port);

    // Set non-blocking mode
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(udpSocket_, FIONBIO, &mode);
#else
    int flags = fcntl(udpSocket_, F_GETFL, 0);
    fcntl(udpSocket_, F_SETFL, flags | O_NONBLOCK);
#endif

    // Create backup directory
    if (!file::createDirectory(config_.backupDir)) {
        LOG_WARN("Backup directory '{}' could not be created (may already exist)", 
                 config_.backupDir);
    }

    // Initialize HTTP server if enabled
    if (config_.enableHttpServer) {
        HttpServerConfig httpConfig;
        httpConfig.port = config_.httpPort;
        httpConfig.backupDir = config_.backupDir;
        httpConfig.staticDir = config_.staticDir;
        httpConfig.maxUploadSize = config_.maxFileSize;

        httpServer_ = std::make_unique<HttpServer>(httpConfig);
        
        // Set up callbacks for file operations
        httpServer_->setFileListCallback([this]() {
            return file::listDirectory(config_.backupDir);
        });
        
        httpServer_->setFileDeleteCallback([this](const std::string& filename) {
            std::string filepath = utils::joinPath(config_.backupDir, filename);
            return file::deleteFile(filepath);
        });
        
        httpServer_->setFileRenameCallback([this](const std::string& oldName, 
                                                   const std::string& newName) {
            std::string oldPath = utils::joinPath(config_.backupDir, oldName);
            std::string newPath = utils::joinPath(config_.backupDir, newName);
            return file::renameFile(oldPath, newPath);
        });
    }

    LOG_INFO("Server initialized on UDP port {}", actualUdpPort_);
    return ErrorCode::SUCCESS;
}

ErrorCode Server::run() {
    running_.store(true);
    state_ = ServerState::WAITING;

    // Start HTTP server if enabled
    if (httpServer_) {
        if (httpServer_->start()) {
            LOG_INFO("HTTP management server started on port {}", config_.httpPort);
            std::cout << "HTTP management server @ http://0.0.0.0:" << config_.httpPort << std::endl;
        } else {
            LOG_ERROR("Failed to start HTTP management server");
        }
    }

    printBanner();
    printWaiting();

    protocol::CmdMsg cmdMsg;
    
    while (!shutdownRequested_.load()) {
        // Use select for timeout-based polling
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(udpSocket_, &readfds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;  // 100ms

#ifdef _WIN32
        int selectResult = select(0, &readfds, nullptr, nullptr, &tv);
#else
        int selectResult = select(udpSocket_ + 1, &readfds, nullptr, nullptr, &tv);
#endif

        if (selectResult < 0) {
            LOG_ERROR("select() failed");
            continue;
        }

        if (selectResult == 0) {
            // Timeout, continue checking for shutdown
            continue;
        }

        // Receive command
        std::memset(&cmdMsg, 0, sizeof(cmdMsg));
        clientAddrLen_ = sizeof(currentClientAddr_);

        ssize_t recvLen = recvfrom(udpSocket_, 
                                    reinterpret_cast<char*>(&cmdMsg), 
                                    sizeof(cmdMsg), 
                                    0,
                                    reinterpret_cast<struct sockaddr*>(&currentClientAddr_),
                                    &clientAddrLen_);

        if (recvLen > 0) {
            LOG_INFO("[CMD RECEIVED]: {}", protocol::commandToString(cmdMsg.getCommand()));
            std::cout << "[CMD RECEIVED]: " << protocol::commandToString(cmdMsg.getCommand()) 
                      << std::endl;

            // Record request metric
            Metrics::instance().recordRequest(protocol::commandToString(cmdMsg.getCommand()));

            ErrorCode result = processCommand(cmdMsg, currentClientAddr_);
            if (result != ErrorCode::SUCCESS && 
                cmdMsg.getCommand() != protocol::Command::SHUTDOWN) {
                LOG_WARN("Command processing failed: {}", errorCodeToString(result));
                // Record error metric
                Metrics::instance().recordError(
                    protocol::commandToString(cmdMsg.getCommand()),
                    errorCodeToString(result));
            }

            if (state_ == ServerState::SHUTDOWN) {
                break;
            }

            state_ = ServerState::WAITING;
            printWaiting();
        }
    }

    // Stop HTTP server
    if (httpServer_) {
        httpServer_->stop();
    }

    running_.store(false);
    LOG_INFO("Server shutdown complete");
    return ErrorCode::SUCCESS;
}

void Server::requestShutdown() {
    shutdownRequested_.store(true);
}

ErrorCode Server::processCommand(const protocol::CmdMsg& cmd, 
                                  const struct sockaddr_in& clientAddr) {
    utils::Timer timer;
    ErrorCode result = ErrorCode::SUCCESS;
    
    switch (cmd.getCommand()) {
        case protocol::Command::LS:
            state_ = ServerState::PROCESS_LS;
            std::cout << "Received LS command" << std::endl;
            result = handleLs(clientAddr);
            break;

        case protocol::Command::SEND:
            state_ = ServerState::PROCESS_SEND;
            std::cout << "Received SEND command" << std::endl;
            result = handleSend(cmd, clientAddr);
            break;

        case protocol::Command::REMOVE:
            state_ = ServerState::PROCESS_REMOVE;
            std::cout << "Received REMOVE command" << std::endl;
            result = handleRemove(cmd, clientAddr);
            break;

        case protocol::Command::RENAME:
            state_ = ServerState::PROCESS_RENAME;
            std::cout << "Received RENAME command" << std::endl;
            result = handleRename(cmd, clientAddr);
            break;

        case protocol::Command::SHUTDOWN:
            state_ = ServerState::SHUTDOWN;
            std::cout << "Received SHUTDOWN command" << std::endl;
            result = handleShutdown(clientAddr);
            break;

        case protocol::Command::OVERWRITE:
            // Handled within SEND flow
            LOG_DEBUG("Received OVERWRITE command (should be handled in SEND flow)");
            break;

        default:
            LOG_WARN("Unknown command received: {}", static_cast<int>(cmd.cmd));
            result = ErrorCode::INVALID_COMMAND;
            break;
    }

    LOG_CMD(protocol::commandToString(cmd.getCommand()), 
            cmd.filename, 
            static_cast<int>(result), 
            timer.elapsedMs());

    return result;
}

ErrorCode Server::handleLs(const struct sockaddr_in& clientAddr) {
    std::cout << "Processing LS command" << std::endl;
    
    auto files = file::listDirectory(config_.backupDir);
    
    if (files.empty()) {
        std::cout << " - server backup folder is empty." << std::endl;
        LOG_INFO("Backup folder is empty");
    } else {
        for (const auto& f : files) {
            std::cout << " - " << f << std::endl;
        }
        LOG_INFO("Listed {} files", files.size());
    }

    // Build data message with file list
    protocol::DataMsg dataMsg;
    int offset = 0;
    for (const auto& filename : files) {
        if (offset + filename.length() + 1 < protocol::DATA_BUF_LEN) {
            std::strcpy(dataMsg.data + offset, filename.c_str());
            offset += static_cast<int>(filename.length()) + 1;
        }
    }

    return sendDataResponse(dataMsg, clientAddr);
}

ErrorCode Server::handleSend(const protocol::CmdMsg& cmd, 
                             const struct sockaddr_in& clientAddr) {
    std::cout << "Processing SEND command" << std::endl;
    std::cout << " - filename: " << cmd.filename << std::endl;
    std::cout << " - filesize: " << cmd.size << std::endl;

    std::string filename = cmd.filename;
    size_t filesize = cmd.size;

    // Validate filename
    if (!utils::isValidFilename(filename)) {
        LOG_WARN("Invalid filename rejected: {}", filename);
        protocol::CmdMsg response;
        response.setCommand(protocol::Command::ACK);
        response.error = static_cast<uint16_t>(ErrorCode::INVALID_FILENAME);
        return sendResponse(response, clientAddr);
    }

    std::string filepath = utils::joinPath(config_.backupDir, filename);

    // Check if file exists
    bool fileExists = file::fileExists(filepath);
    LOG_DEBUG("File exists check for '{}': {}", filepath, fileExists);

    if (fileExists) {
        // Notify client that file exists
        protocol::CmdMsg response;
        response.setCommand(protocol::Command::FILE_EXISTS);
        response.error = 0;
        response.size = static_cast<uint32_t>(filesize);

        std::cout << " - file exists; waiting for overwrite confirmation" << std::endl;
        LOG_INFO("File '{}' exists, asking for overwrite confirmation", filename);

        ErrorCode sendResult = sendResponse(response, clientAddr);
        if (sendResult != ErrorCode::SUCCESS) {
            return sendResult;
        }

        // Wait for overwrite confirmation
        protocol::CmdMsg confirmMsg;
        socklen_t addrLen = sizeof(struct sockaddr_in);
        
        // Set timeout for receiving confirmation
        struct timeval tv;
        tv.tv_sec = config_.recvTimeoutMs / 1000;
        tv.tv_usec = (config_.recvTimeoutMs % 1000) * 1000;
        setsockopt(udpSocket_, SOL_SOCKET, SO_RCVTIMEO, 
                   reinterpret_cast<const char*>(&tv), sizeof(tv));

        ssize_t recvLen = recvfrom(udpSocket_, 
                                    reinterpret_cast<char*>(&confirmMsg),
                                    sizeof(confirmMsg),
                                    0,
                                    reinterpret_cast<struct sockaddr*>(&currentClientAddr_),
                                    &addrLen);

        if (recvLen <= 0) {
            LOG_WARN("Timeout waiting for overwrite confirmation");
            return ErrorCode::TIMEOUT;
        }

        if (confirmMsg.getCommand() != protocol::Command::OVERWRITE || 
            confirmMsg.error != 0) {
            std::cout << " - client chose not to overwrite file." << std::endl;
            LOG_INFO("Client declined to overwrite file");
            
            protocol::CmdMsg ackMsg;
            ackMsg.setCommand(protocol::Command::ACK);
            ackMsg.error = 2;  // Cancelled
            return sendResponse(ackMsg, clientAddr);
        }

        std::cout << " - client chose to overwrite file." << std::endl;
        LOG_INFO("Client confirmed overwrite");
        
        // Update filesize and filename from confirm message
        filesize = confirmMsg.size;
        filename = confirmMsg.filename;
        filepath = utils::joinPath(config_.backupDir, filename);
    }

    // Receive file based on size
    ErrorCode result;
    if (filesize <= protocol::DATA_BUF_LEN) {
        result = receiveSmallFile(filepath, filesize, clientAddr);
    } else {
        result = receiveLargeFile(filepath, filesize, clientAddr);
    }

    return result;
}

ErrorCode Server::receiveSmallFile(const std::string& filepath, size_t filesize,
                                   const struct sockaddr_in& clientAddr) {
    // For new files (not overwrite), send ACK first
    protocol::CmdMsg ackMsg;
    ackMsg.setCommand(protocol::Command::ACK);
    ackMsg.error = 0;
    sendResponse(ackMsg, clientAddr);

    // Receive file data via UDP
    protocol::DataMsg dataMsg;
    socklen_t addrLen = sizeof(struct sockaddr_in);
    
    struct timeval tv;
    tv.tv_sec = config_.recvTimeoutMs / 1000;
    tv.tv_usec = (config_.recvTimeoutMs % 1000) * 1000;
    setsockopt(udpSocket_, SOL_SOCKET, SO_RCVTIMEO, 
               reinterpret_cast<const char*>(&tv), sizeof(tv));

    ssize_t recvLen = recvfrom(udpSocket_,
                                reinterpret_cast<char*>(&dataMsg),
                                sizeof(dataMsg),
                                0,
                                reinterpret_cast<struct sockaddr*>(&currentClientAddr_),
                                &addrLen);

    if (recvLen <= 0) {
        LOG_ERROR("Failed to receive file data");
        ackMsg.error = 1;
        sendResponse(ackMsg, clientAddr);
        return ErrorCode::SOCKET_RECV_ERROR;
    }

    // Write file
    std::ofstream file(filepath, std::ios::binary);
    if (!file) {
        LOG_ERROR("Failed to create file: {}", filepath);
        ackMsg.error = 1;
        sendResponse(ackMsg, clientAddr);
        return ErrorCode::FILE_OPEN_ERROR;
    }

    file.write(dataMsg.data, filesize);
    file.close();

    // Record transfer metrics
    Metrics::instance().recordBytesReceived(filesize);
    Metrics::instance().recordSendSession();

    std::cout << " - " << utils::getFilename(filepath) << " has been received." << std::endl;
    LOG_INFO("File '{}' received ({} bytes)", filepath, filesize);

    ackMsg.setCommand(protocol::Command::ACK);
    ackMsg.error = 0;
    return sendResponse(ackMsg, clientAddr);
}

ErrorCode Server::receiveLargeFile(const std::string& filepath, size_t filesize,
                                   const struct sockaddr_in& clientAddr) {
    // Create TCP socket
    socket_t tcpSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tcpSocket == INVALID_SOCKET_VALUE) {
        LOG_ERROR("Failed to create TCP socket");
        return ErrorCode::SOCKET_CREATE_ERROR;
    }

    int optval = 1;
    setsockopt(tcpSocket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&optval), sizeof(optval));

    // Bind TCP socket
    struct sockaddr_in tcpAddr;
    std::memset(&tcpAddr, 0, sizeof(tcpAddr));
    tcpAddr.sin_family = AF_INET;
    tcpAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    tcpAddr.sin_port = htons(currentTcpPort_);

    if (bind(tcpSocket, reinterpret_cast<struct sockaddr*>(&tcpAddr), 
             sizeof(tcpAddr)) < 0) {
        LOG_ERROR("Failed to bind TCP socket to port {}", currentTcpPort_);
        closesocket(tcpSocket);
        return ErrorCode::SOCKET_BIND_ERROR;
    }

    if (listen(tcpSocket, 5) < 0) {
        LOG_ERROR("Failed to listen on TCP socket");
        closesocket(tcpSocket);
        return ErrorCode::SOCKET_LISTEN_ERROR;
    }

    // Send TCP port to client
    protocol::CmdMsg ackMsg;
    ackMsg.setCommand(protocol::Command::ACK);
    ackMsg.error = 0;
    ackMsg.size = currentTcpPort_;

    std::cout << " - listen @: " << currentTcpPort_ << std::endl;
    LOG_INFO("TCP listening on port {}", currentTcpPort_);

    ErrorCode sendResult = sendResponse(ackMsg, clientAddr);
    if (sendResult != ErrorCode::SUCCESS) {
        closesocket(tcpSocket);
        return sendResult;
    }

    // Accept connection
    struct sockaddr_in clientTcpAddr;
    socklen_t clientTcpLen = sizeof(clientTcpAddr);
    socket_t clientTcpSocket = accept(tcpSocket,
                                       reinterpret_cast<struct sockaddr*>(&clientTcpAddr),
                                       &clientTcpLen);

    if (clientTcpSocket == INVALID_SOCKET_VALUE) {
        LOG_ERROR("Failed to accept TCP connection");
        closesocket(tcpSocket);
        return ErrorCode::SOCKET_ACCEPT_ERROR;
    }

    std::cout << " - connected with client." << std::endl;
    LOG_INFO("TCP connection accepted");

    // Receive file data
    std::ofstream file(filepath, std::ios::binary);
    if (!file) {
        LOG_ERROR("Failed to create file: {}", filepath);
        closesocket(clientTcpSocket);
        closesocket(tcpSocket);
        return ErrorCode::FILE_OPEN_ERROR;
    }

    constexpr size_t BUFFER_SIZE = 3000;
    char buffer[BUFFER_SIZE];
    size_t totalBytes = 0;

    while (totalBytes < filesize) {
        ssize_t bytesReceived = recv(clientTcpSocket, buffer, BUFFER_SIZE, 0);

        if (bytesReceived <= 0) {
            break;
        }

        file.write(buffer, bytesReceived);
        totalBytes += bytesReceived;

        std::cout << bytesReceived << std::endl;
        std::cout << " - total bytes received: " << totalBytes << std::endl;
    }

    file.close();
    closesocket(clientTcpSocket);
    closesocket(tcpSocket);

    // Record transfer metrics
    Metrics::instance().recordBytesReceived(totalBytes);
    Metrics::instance().recordSendSession();

    std::cout << " - " << utils::getFilename(filepath) << " has been received." << std::endl;
    std::cout << " - send acknowledgemet." << std::endl;
    LOG_TRANSFER(filepath, totalBytes, "receive", 0);

    currentTcpPort_++;  // Increment for next transfer

    // Send final acknowledgement
    ackMsg.setCommand(protocol::Command::ACK);
    ackMsg.error = 0;
    return sendResponse(ackMsg, clientAddr);
}

ErrorCode Server::handleRemove(const protocol::CmdMsg& cmd,
                               const struct sockaddr_in& clientAddr) {
    std::cout << "Processing REMOVE command" << std::endl;

    std::string filename = cmd.filename;

    // Validate filename
    if (!utils::isValidFilename(filename)) {
        LOG_WARN("Invalid filename rejected: {}", filename);
        protocol::CmdMsg response;
        response.setCommand(protocol::Command::ACK);
        response.error = static_cast<uint16_t>(ErrorCode::INVALID_FILENAME);
        return sendResponse(response, clientAddr);
    }

    std::string filepath = utils::joinPath(config_.backupDir, filename);

    protocol::CmdMsg response;
    response.setCommand(protocol::Command::ACK);

    if (!file::fileExists(filepath)) {
        std::cout << " - file " << filename << " does not exist." << std::endl;
        LOG_INFO("Remove failed: file '{}' not found", filename);
        response.error = 1;
    } else if (file::deleteFile(filepath)) {
        std::cout << " - ./backup/" << filename << " has been removed." << std::endl;
        std::cout << " - send acknowledgemet." << std::endl;
        LOG_INFO("File '{}' removed", filename);
        response.error = 0;
    } else {
        std::cout << "Failed to delete file " << filename << std::endl;
        LOG_ERROR("Failed to delete file '{}'", filename);
        response.error = 1;
    }

    return sendResponse(response, clientAddr);
}

ErrorCode Server::handleRename(const protocol::CmdMsg& cmd,
                               const struct sockaddr_in& clientAddr) {
    std::cout << "Processing RENAME command" << std::endl;

    std::string oldFilename = cmd.filename;

    // Validate old filename
    if (!utils::isValidFilename(oldFilename)) {
        LOG_WARN("Invalid old filename rejected: {}", oldFilename);
        protocol::CmdMsg response;
        response.setCommand(protocol::Command::ACK);
        response.error = static_cast<uint16_t>(ErrorCode::INVALID_FILENAME);
        return sendResponse(response, clientAddr);
    }

    std::string oldFilepath = utils::joinPath(config_.backupDir, oldFilename);

    // Check if source file exists
    if (!file::fileExists(oldFilepath)) {
        std::cout << " - file " << oldFilename << " does not exist." << std::endl;
        LOG_INFO("Rename failed: source file '{}' not found", oldFilename);
        
        protocol::CmdMsg response;
        response.setCommand(protocol::Command::ACK);
        response.error = 1;
        return sendResponse(response, clientAddr);
    }

    // ===== ENHANCED HANDSHAKE FOR RENAME =====
    // Send READY_FOR_NEW_NAME to explicitly signal we're ready for the new filename
    if (config_.enableHandshake) {
        protocol::CmdMsg readyMsg;
        readyMsg.setCommand(protocol::Command::READY_FOR_NEW_NAME);
        readyMsg.error = 0;
        sendResponse(readyMsg, clientAddr);
        LOG_DEBUG("Sent READY_FOR_NEW_NAME handshake");
    }

    // Receive new filename
    protocol::DataMsg dataMsg;
    socklen_t addrLen = sizeof(struct sockaddr_in);
    
    struct timeval tv;
    tv.tv_sec = config_.recvTimeoutMs / 1000;
    tv.tv_usec = (config_.recvTimeoutMs % 1000) * 1000;
    setsockopt(udpSocket_, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&tv), sizeof(tv));

    ssize_t recvLen = recvfrom(udpSocket_,
                                reinterpret_cast<char*>(&dataMsg),
                                sizeof(dataMsg),
                                0,
                                reinterpret_cast<struct sockaddr*>(&currentClientAddr_),
                                &addrLen);

    if (recvLen <= 0) {
        LOG_ERROR("Failed to receive new filename");
        return ErrorCode::TIMEOUT;
    }

    std::string newFilename = dataMsg.data;

    // Validate new filename
    if (!utils::isValidFilename(newFilename)) {
        LOG_WARN("Invalid new filename rejected: {}", newFilename);
        protocol::CmdMsg response;
        response.setCommand(protocol::Command::ACK);
        response.error = static_cast<uint16_t>(ErrorCode::INVALID_FILENAME);
        return sendResponse(response, clientAddr);
    }

    std::string newFilepath = utils::joinPath(config_.backupDir, newFilename);

    // Check if target file exists
    if (file::fileExists(newFilepath)) {
        std::cout << " - target file " << newFilename 
                  << " already exists. Asking for confirmation." << std::endl;
        LOG_INFO("Target file '{}' exists, asking for confirmation", newFilename);

        protocol::CmdMsg existsMsg;
        existsMsg.setCommand(protocol::Command::FILE_EXISTS);
        existsMsg.error = 0;
        sendResponse(existsMsg, clientAddr);

        // Wait for overwrite confirmation
        protocol::CmdMsg confirmMsg;
        recvLen = recvfrom(udpSocket_,
                            reinterpret_cast<char*>(&confirmMsg),
                            sizeof(confirmMsg),
                            0,
                            reinterpret_cast<struct sockaddr*>(&currentClientAddr_),
                            &addrLen);

        if (recvLen <= 0) {
            LOG_WARN("Timeout waiting for rename overwrite confirmation");
            return ErrorCode::TIMEOUT;
        }

        if (confirmMsg.getCommand() != protocol::Command::OVERWRITE ||
            confirmMsg.error != 0) {
            std::cout << " - client chose not to overwrite target file." << std::endl;
            LOG_INFO("Client declined rename overwrite");
            
            protocol::CmdMsg ackMsg;
            ackMsg.setCommand(protocol::Command::ACK);
            ackMsg.error = 2;  // Cancelled
            return sendResponse(ackMsg, clientAddr);
        }

        // Delete existing target file
        file::deleteFile(newFilepath);
        std::cout << " - client chose to overwrite target file." << std::endl;
        LOG_INFO("Client confirmed rename overwrite");
    }

    // Perform rename
    protocol::CmdMsg response;
    response.setCommand(protocol::Command::ACK);

    if (file::renameFile(oldFilepath, newFilepath)) {
        std::cout << " -the file has been renamed to " << newFilename << "." << std::endl;
        std::cout << " -send acknowledgement" << std::endl;
        LOG_INFO("File renamed from '{}' to '{}'", oldFilename, newFilename);
        response.error = 0;
    } else {
        std::cout << "Failed to rename file from " << oldFilename 
                  << " to " << newFilename << std::endl;
        LOG_ERROR("Failed to rename '{}' to '{}'", oldFilename, newFilename);
        response.error = 1;
    }

    return sendResponse(response, clientAddr);
}

ErrorCode Server::handleShutdown(const struct sockaddr_in& clientAddr) {
    std::cout << "Processing SHUTDOWN command" << std::endl;
    std::cout << " - send acknowledgemet." << std::endl;
    LOG_INFO("Server shutdown requested");

    protocol::CmdMsg response;
    response.setCommand(protocol::Command::ACK);
    response.error = 0;
    sendResponse(response, clientAddr);

    shutdownRequested_.store(true);
    return ErrorCode::SUCCESS;
}

ErrorCode Server::sendResponse(const protocol::CmdMsg& response,
                               const struct sockaddr_in& clientAddr) {
    ssize_t sentLen = sendto(udpSocket_,
                              reinterpret_cast<const char*>(&response),
                              sizeof(response),
                              0,
                              reinterpret_cast<const struct sockaddr*>(&clientAddr),
                              sizeof(clientAddr));

    if (sentLen < 0) {
        LOG_ERROR("Failed to send response");
        return ErrorCode::SOCKET_SEND_ERROR;
    }

    return ErrorCode::SUCCESS;
}

ErrorCode Server::sendDataResponse(const protocol::DataMsg& data,
                                   const struct sockaddr_in& clientAddr) {
    ssize_t sentLen = sendto(udpSocket_,
                              reinterpret_cast<const char*>(&data),
                              sizeof(data),
                              0,
                              reinterpret_cast<const struct sockaddr*>(&clientAddr),
                              sizeof(clientAddr));

    if (sentLen < 0) {
        LOG_ERROR("Failed to send data response");
        return ErrorCode::SOCKET_SEND_ERROR;
    }

    return ErrorCode::SUCCESS;
}

void Server::printBanner() const {
    std::cout << "**************************************************************" << std::endl;
    std::cout << "Reliable Remote Backup System - Server v1.0" << std::endl;
    std::cout << "**************************************************************" << std::endl;
}

void Server::printWaiting() const {
    std::cout << "**************************************************************" << std::endl;
    std::cout << "Waiting UDP command @ port number: " << actualUdpPort_ << std::endl;
    std::cout << "**************************************************************" << std::endl;
}

}  // namespace server
}  // namespace backup
