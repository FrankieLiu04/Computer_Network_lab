#include "client/client.h"
#include "common/logger.h"
#include "common/utils.h"
#include "common/file_utils.h"

#include <iostream>
#include <fstream>
#include <cstring>
#include <sstream>
#include <algorithm>

namespace backup {
namespace client {

Client::Client(const ClientConfig& config) : config_(config) {
    std::memset(&serverAddr_, 0, sizeof(serverAddr_));
}

Client::~Client() {
    if (udpSocket_ != INVALID_SOCKET_VALUE) {
        closesocket(udpSocket_);
    }

#ifdef _WIN32
    if (wsaInitialized_) {
        WSACleanup();
    }
#endif
}

ErrorCode Client::initialize() {
#ifdef _WIN32
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

    // Resolve server address
    serverAddr_.sin_family = AF_INET;
    serverAddr_.sin_port = htons(config_.serverPort);

#ifdef _WIN32
    struct addrinfo hints, *result;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(config_.serverHost.c_str(), nullptr, &hints, &result) != 0) {
        LOG_ERROR("Failed to resolve host: {}", config_.serverHost);
        return ErrorCode::SOCKET_CONNECT_ERROR;
    }

    serverAddr_.sin_addr = reinterpret_cast<struct sockaddr_in*>(result->ai_addr)->sin_addr;
    freeaddrinfo(result);
#else
    struct hostent* hostInfo = gethostbyname(config_.serverHost.c_str());
    if (hostInfo == nullptr) {
        LOG_ERROR("Failed to resolve host: {}", config_.serverHost);
        return ErrorCode::SOCKET_CONNECT_ERROR;
    }
    std::memcpy(&serverAddr_.sin_addr, hostInfo->h_addr, hostInfo->h_length);
#endif

    // Set receive timeout
    struct timeval tv;
    tv.tv_sec = config_.recvTimeoutMs / 1000;
    tv.tv_usec = (config_.recvTimeoutMs % 1000) * 1000;
    setsockopt(udpSocket_, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&tv), sizeof(tv));

    LOG_INFO("Client initialized, connecting to {}:{}", 
             config_.serverHost, config_.serverPort);
    return ErrorCode::SUCCESS;
}

ErrorCode Client::run() {
    running_ = true;
    state_ = ClientState::WAITING;

    std::cout << "**************************************************************" << std::endl;
    std::cout << "Reliable Remote Backup System - Client v1.0" << std::endl;
    std::cout << "Successfully connected to server. Type your command." << std::endl;
    std::cout << "**************************************************************" << std::endl;

    std::string line;
    while (running_ && state_ != ClientState::QUIT) {
        std::cout << "$ ";
        std::cout.flush();

        if (!std::getline(std::cin, line)) {
            break;
        }

        line = utils::trim(line);
        if (line.empty()) {
            continue;
        }

        ErrorCode result = executeCommand(line);
        if (result != ErrorCode::SUCCESS && result != ErrorCode::CANCELLED) {
            LOG_WARN("Command failed: {}", errorCodeToString(result));
        }

        if (state_ == ClientState::QUIT) {
            break;
        }

        state_ = ClientState::WAITING;
    }

    running_ = false;
    return ErrorCode::SUCCESS;
}

Client::ParsedCommand Client::parseCommand(const std::string& input) {
    ParsedCommand parsed;
    std::istringstream iss(input);
    iss >> parsed.command >> parsed.arg1 >> parsed.arg2;

    // Convert command to lowercase
    std::transform(parsed.command.begin(), parsed.command.end(), 
                   parsed.command.begin(), ::tolower);

    return parsed;
}

ErrorCode Client::executeCommand(const std::string& input) {
    ParsedCommand parsed = parseCommand(input);

    if (parsed.command == "ls") {
        state_ = ClientState::PROCESS_LS;
        return handleLs();
    } else if (parsed.command == "send") {
        if (parsed.arg1.empty()) {
            std::cout << " - missing filename" << std::endl;
            return ErrorCode::INVALID_ARGUMENT;
        }
        state_ = ClientState::PROCESS_SEND;
        return handleSend(parsed.arg1);
    } else if (parsed.command == "remove") {
        if (parsed.arg1.empty()) {
            std::cout << " - missing filename" << std::endl;
            return ErrorCode::INVALID_ARGUMENT;
        }
        state_ = ClientState::PROCESS_REMOVE;
        return handleRemove(parsed.arg1);
    } else if (parsed.command == "rename") {
        if (parsed.arg1.empty() || parsed.arg2.empty()) {
            std::cout << " - missing filename(s)" << std::endl;
            return ErrorCode::INVALID_ARGUMENT;
        }
        state_ = ClientState::PROCESS_RENAME;
        return handleRename(parsed.arg1, parsed.arg2);
    } else if (parsed.command == "shutdown") {
        state_ = ClientState::SHUTDOWN;
        return handleShutdown();
    } else if (parsed.command == "quit" || parsed.command == "exit") {
        state_ = ClientState::QUIT;
        std::cout << "Exiting client" << std::endl;
        return ErrorCode::SUCCESS;
    } else {
        std::cout << " - wrong command." << std::endl;
        return ErrorCode::INVALID_COMMAND;
    }
}

ErrorCode Client::handleLs() {
    protocol::CmdMsg cmd;
    cmd.setCommand(protocol::Command::LS);
    cmd.size = 0;
    cmd.error = 0;

    ErrorCode result = sendCommand(cmd);
    if (result != ErrorCode::SUCCESS) {
        std::cerr << "Error sending LS command" << std::endl;
        return result;
    }

    protocol::DataMsg dataMsg;
    result = receiveData(dataMsg);
    if (result != ErrorCode::SUCCESS) {
        std::cerr << "Timeout or error receiving LS response" << std::endl;
        return result;
    }

    if (dataMsg.data[0] == '\0') {
        std::cout << " - server backup folder is empty." << std::endl;
    } else {
        std::cout << "Files on server:" << std::endl;
        const char* file = dataMsg.data;
        while (*file) {
            std::cout << " - " << file << std::endl;
            file += std::strlen(file) + 1;
        }
    }

    return ErrorCode::SUCCESS;
}

ErrorCode Client::handleSend(const std::string& filename) {
    // Check if local file exists
    if (!file::fileExists(filename)) {
        std::cout << " - cannot open file: " << filename << std::endl;
        return ErrorCode::FILE_NOT_FOUND;
    }

    size_t filesize = file::getFileSize(filename);
    std::cout << " - filesize:" << filesize << std::endl;

    // Validate filename
    std::string baseName = utils::getFilename(filename);
    if (!utils::isValidFilename(baseName)) {
        std::cout << " - invalid filename" << std::endl;
        return ErrorCode::INVALID_FILENAME;
    }

    // Send SEND command
    protocol::CmdMsg cmd;
    cmd.setCommand(protocol::Command::SEND);
    cmd.setFilename(baseName.c_str());
    cmd.size = static_cast<uint32_t>(filesize);
    cmd.error = 0;

    ErrorCode result = sendCommand(cmd);
    if (result != ErrorCode::SUCCESS) {
        std::cerr << "Error sending command" << std::endl;
        return result;
    }

    // Receive response
    protocol::CmdMsg response;
    result = receiveResponse(response);
    if (result != ErrorCode::SUCCESS) {
        std::cerr << "Timeout or error receiving response" << std::endl;
        return result;
    }

    LOG_DEBUG("Received command: {}, error: {}, size: {}", 
              static_cast<int>(response.cmd), response.error, response.size);

    // Handle file exists case
    if (response.getCommand() == protocol::Command::FILE_EXISTS) {
        std::cout << " - File " << baseName << " already exists on server. Overwrite? (y/n): ";
        std::string answer;
        std::cin >> answer;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        protocol::CmdMsg confirmMsg;
        confirmMsg.setCommand(protocol::Command::OVERWRITE);
        confirmMsg.setFilename(baseName.c_str());
        confirmMsg.size = static_cast<uint32_t>(filesize);

        if (std::tolower(answer[0]) == 'y') {
            confirmMsg.error = 0;
        } else {
            confirmMsg.error = 1;
        }

        result = sendCommand(confirmMsg);
        if (result != ErrorCode::SUCCESS) {
            return result;
        }

        if (std::tolower(answer[0]) != 'y') {
            receiveResponse(response);  // Consume the ACK
            std::cout << " - File transfer cancelled." << std::endl;
            return ErrorCode::CANCELLED;
        }

        // Wait for server confirmation
        result = receiveResponse(response);
        if (result != ErrorCode::SUCCESS) {
            return result;
        }

        if (response.getCommand() != protocol::Command::ACK) {
            std::cerr << "Unexpected server response" << std::endl;
            return ErrorCode::UNEXPECTED_RESPONSE;
        }

        std::cout << " - Server ready to receive file" << std::endl;

        if (filesize > protocol::DATA_BUF_LEN) {
            uint16_t tcpPort = static_cast<uint16_t>(response.size);
            std::cout << " - TCP port:" << tcpPort << std::endl;
            return sendLargeFile(filename, filesize, tcpPort);
        }
    } else if (response.getCommand() == protocol::Command::ACK) {
        if (filesize > protocol::DATA_BUF_LEN) {
            uint16_t tcpPort = static_cast<uint16_t>(response.size);
            std::cout << " - TCP port:" << tcpPort << std::endl;
            return sendLargeFile(filename, filesize, tcpPort);
        }
    }

    // Small file: send via UDP
    return sendSmallFile(filename, filesize);
}

ErrorCode Client::sendSmallFile(const std::string& filepath, size_t filesize) {
    protocol::DataMsg dataMsg;
    
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        return ErrorCode::FILE_OPEN_ERROR;
    }
    
    file.read(dataMsg.data, filesize);
    file.close();

    ErrorCode result = sendData(dataMsg);
    if (result != ErrorCode::SUCCESS) {
        std::cerr << "Error sending file data" << std::endl;
        return result;
    }

    protocol::CmdMsg ackMsg;
    result = receiveResponse(ackMsg);
    if (result != ErrorCode::SUCCESS) {
        std::cerr << "Timeout or error receiving response" << std::endl;
        return result;
    }

    if (ackMsg.getCommand() == protocol::Command::ACK && ackMsg.error == 0) {
        std::cout << " - file transmission is completed." << std::endl;
        return ErrorCode::SUCCESS;
    } else {
        std::cerr << " - file transmission is failed" << std::endl;
        return ErrorCode::FILE_WRITE_ERROR;
    }
}

ErrorCode Client::sendLargeFile(const std::string& filepath, size_t filesize, 
                                 uint16_t tcpPort) {
    // Create TCP socket
    socket_t tcpSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tcpSocket == INVALID_SOCKET_VALUE) {
        std::cerr << "Error creating TCP socket" << std::endl;
        return ErrorCode::SOCKET_CREATE_ERROR;
    }

    // Connect to server
    struct sockaddr_in tcpServerAddr = serverAddr_;
    tcpServerAddr.sin_port = htons(tcpPort);

    if (connect(tcpSocket, reinterpret_cast<struct sockaddr*>(&tcpServerAddr),
                sizeof(tcpServerAddr)) < 0) {
        std::cerr << "Error connecting to TCP server" << std::endl;
        closesocket(tcpSocket);
        return ErrorCode::SOCKET_CONNECT_ERROR;
    }

    // Send file
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        closesocket(tcpSocket);
        return ErrorCode::FILE_OPEN_ERROR;
    }

    constexpr size_t BUFFER_SIZE = 3000;
    char buffer[BUFFER_SIZE];
    size_t bytesSent = 0;
    int segmentCount = 0;

    while (bytesSent < filesize) {
        size_t bufferSize = std::min(BUFFER_SIZE, filesize - bytesSent);
        file.read(buffer, bufferSize);

        ssize_t sent = send(tcpSocket, buffer, static_cast<int>(bufferSize), 0);
        if (sent < 0) {
            std::cerr << "Error sending file data" << std::endl;
            file.close();
            closesocket(tcpSocket);
            return ErrorCode::SOCKET_SEND_ERROR;
        }

        bytesSent += sent;
        segmentCount++;

        std::cout << "Buffer size: " << bufferSize << std::endl;
    }

    std::cout << "Total Segment Number is: " << segmentCount << std::endl;

    file.close();
    closesocket(tcpSocket);

    // Wait for acknowledgement
    protocol::CmdMsg ackMsg;
    ErrorCode result = receiveResponse(ackMsg);
    if (result != ErrorCode::SUCCESS) {
        std::cerr << "Timeout or error receiving completion confirmation" << std::endl;
        return result;
    }

    if (ackMsg.getCommand() == protocol::Command::ACK && ackMsg.error == 0) {
        std::cout << " - file transmission is completed." << std::endl;
        return ErrorCode::SUCCESS;
    } else {
        std::cerr << "Failed to complete file transfer" << std::endl;
        return ErrorCode::FILE_WRITE_ERROR;
    }
}

ErrorCode Client::handleRemove(const std::string& filename) {
    protocol::CmdMsg cmd;
    cmd.setCommand(protocol::Command::REMOVE);
    cmd.setFilename(filename.c_str());
    cmd.size = 0;
    cmd.error = 0;

    ErrorCode result = sendCommand(cmd);
    if (result != ErrorCode::SUCCESS) {
        std::cerr << "Error sending delete command" << std::endl;
        return result;
    }

    protocol::CmdMsg ackMsg;
    result = receiveResponse(ackMsg);
    if (result != ErrorCode::SUCCESS) {
        std::cerr << "Timeout or error receiving response" << std::endl;
        return result;
    }

    if (ackMsg.getCommand() == protocol::Command::ACK) {
        if (ackMsg.error == 0) {
            std::cout << " - file is removed." << std::endl;
            return ErrorCode::SUCCESS;
        } else if (ackMsg.error == 1) {
            std::cout << " - file does not exist on server." << std::endl;
            return ErrorCode::FILE_NOT_FOUND;
        }
    }

    std::cerr << "Failed to delete file" << std::endl;
    return ErrorCode::FILE_DELETE_ERROR;
}

ErrorCode Client::handleRename(const std::string& oldName, const std::string& newName) {
    protocol::CmdMsg cmd;
    cmd.setCommand(protocol::Command::RENAME);
    cmd.setFilename(oldName.c_str());
    cmd.size = 0;
    cmd.error = 0;

    ErrorCode result = sendCommand(cmd);
    if (result != ErrorCode::SUCCESS) {
        std::cerr << "Error sending rename command" << std::endl;
        return result;
    }

    // ===== ENHANCED HANDSHAKE FOR RENAME =====
    // Wait for READY_FOR_NEW_NAME if handshake is enabled
    if (config_.enableHandshake) {
        protocol::CmdMsg readyMsg;
        result = receiveResponse(readyMsg);
        if (result != ErrorCode::SUCCESS) {
            std::cerr << "Timeout or error receiving ready signal" << std::endl;
            return result;
        }

        if (readyMsg.getCommand() == protocol::Command::ACK && readyMsg.error == 1) {
            std::cout << " - source file does not exist on server." << std::endl;
            return ErrorCode::FILE_NOT_FOUND;
        }

        if (readyMsg.getCommand() != protocol::Command::READY_FOR_NEW_NAME) {
            // Fall back to checking if it's an error response
            if (readyMsg.getCommand() == protocol::Command::ACK) {
                if (readyMsg.error == 0) {
                    // Old protocol: rename already completed
                    std::cout << " -file has been renamed." << std::endl;
                    return ErrorCode::SUCCESS;
                } else {
                    std::cout << " - source file does not exist on server." << std::endl;
                    return ErrorCode::FILE_NOT_FOUND;
                }
            }
            LOG_DEBUG("Received unexpected response: {}", static_cast<int>(readyMsg.cmd));
        }
    }

    // Send new filename
    protocol::DataMsg dataMsg;
    dataMsg.setData(newName.c_str(), newName.length());

    result = sendData(dataMsg);
    if (result != ErrorCode::SUCCESS) {
        std::cerr << "Error sending new filename" << std::endl;
        return result;
    }

    // Wait for response
    protocol::CmdMsg ackMsg;
    result = receiveResponse(ackMsg);
    if (result != ErrorCode::SUCCESS) {
        std::cerr << "Timeout or error receiving response" << std::endl;
        return result;
    }

    if (ackMsg.getCommand() == protocol::Command::ACK && ackMsg.error == 1) {
        std::cout << " - source file does not exist on server." << std::endl;
        return ErrorCode::FILE_NOT_FOUND;
    }

    if (ackMsg.getCommand() == protocol::Command::FILE_EXISTS) {
        std::cout << " - File " << newName << " already exists on server. Overwrite? (y/n): ";
        std::string answer;
        std::cin >> answer;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        protocol::CmdMsg confirmMsg;
        confirmMsg.setCommand(protocol::Command::OVERWRITE);
        if (std::tolower(answer[0]) == 'y') {
            confirmMsg.error = 0;
        } else {
            confirmMsg.error = 1;
        }

        result = sendCommand(confirmMsg);
        if (result != ErrorCode::SUCCESS) {
            return result;
        }

        result = receiveResponse(ackMsg);
        if (result != ErrorCode::SUCCESS) {
            return result;
        }

        if (ackMsg.error == 2) {
            std::cout << " - Rename cancelled." << std::endl;
            return ErrorCode::CANCELLED;
        }
    }

    if (ackMsg.getCommand() == protocol::Command::ACK && ackMsg.error == 0) {
        std::cout << " -file has been renamed." << std::endl;
        return ErrorCode::SUCCESS;
    }

    return ErrorCode::FILE_RENAME_ERROR;
}

ErrorCode Client::handleShutdown() {
    protocol::CmdMsg cmd;
    cmd.setCommand(protocol::Command::SHUTDOWN);
    cmd.size = 0;
    cmd.error = 0;

    ErrorCode result = sendCommand(cmd);
    if (result != ErrorCode::SUCCESS) {
        std::cerr << "Error sending shutdown command" << std::endl;
        return result;
    }

    std::cout << " - server is shutdown." << std::endl;
    return ErrorCode::SUCCESS;
}

ErrorCode Client::sendCommand(const protocol::CmdMsg& cmd) {
    ssize_t sentLen = sendto(udpSocket_,
                              reinterpret_cast<const char*>(&cmd),
                              sizeof(cmd),
                              0,
                              reinterpret_cast<struct sockaddr*>(&serverAddr_),
                              sizeof(serverAddr_));

    if (sentLen < 0) {
        LOG_ERROR("Failed to send command");
        return ErrorCode::SOCKET_SEND_ERROR;
    }

    return ErrorCode::SUCCESS;
}

ErrorCode Client::sendData(const protocol::DataMsg& data) {
    ssize_t sentLen = sendto(udpSocket_,
                              reinterpret_cast<const char*>(&data),
                              sizeof(data),
                              0,
                              reinterpret_cast<struct sockaddr*>(&serverAddr_),
                              sizeof(serverAddr_));

    if (sentLen < 0) {
        LOG_ERROR("Failed to send data");
        return ErrorCode::SOCKET_SEND_ERROR;
    }

    return ErrorCode::SUCCESS;
}

ErrorCode Client::receiveResponse(protocol::CmdMsg& response) {
    socklen_t addrLen = sizeof(serverAddr_);
    ssize_t recvLen = recvfrom(udpSocket_,
                                reinterpret_cast<char*>(&response),
                                sizeof(response),
                                0,
                                reinterpret_cast<struct sockaddr*>(&serverAddr_),
                                &addrLen);

    if (recvLen <= 0) {
        return ErrorCode::TIMEOUT;
    }

    return ErrorCode::SUCCESS;
}

ErrorCode Client::receiveData(protocol::DataMsg& data) {
    socklen_t addrLen = sizeof(serverAddr_);
    ssize_t recvLen = recvfrom(udpSocket_,
                                reinterpret_cast<char*>(&data),
                                sizeof(data),
                                0,
                                reinterpret_cast<struct sockaddr*>(&serverAddr_),
                                &addrLen);

    if (recvLen <= 0) {
        return ErrorCode::TIMEOUT;
    }

    return ErrorCode::SUCCESS;
}

}  // namespace client
}  // namespace backup
