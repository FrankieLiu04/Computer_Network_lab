#include "client/client.h"
#include "common/logger.h"
#include <iostream>
#include <cstring>
#include <cstdlib>

using namespace backup;
using namespace backup::client;

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [-address <server_host>] -port <udp_port>" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -address <host>  Server hostname or IP (default: 127.0.0.1)" << std::endl;
    std::cout << "  -port <port>     Server UDP port (required)" << std::endl;
    std::cout << "  -log             Enable file logging" << std::endl;
    std::cout << "  -help            Show this help message" << std::endl;
}

int main(int argc, char* argv[]) {
    ClientConfig config;
    bool enableFileLog = false;
    bool portProvided = false;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "-port") == 0 && i + 1 < argc) {
            config.serverPort = static_cast<uint16_t>(std::atoi(argv[++i]));
            portProvided = true;
        } else if (std::strcmp(argv[i], "-address") == 0 && i + 1 < argc) {
            config.serverHost = argv[++i];
        } else if (std::strcmp(argv[i], "-log") == 0) {
            enableFileLog = true;
        } else if (std::strcmp(argv[i], "-help") == 0 || std::strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << argv[i] << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    if (!portProvided) {
        std::cerr << "Error: -port is required" << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    // Initialize logger
    Logger::init("client", enableFileLog, "logs");
    LOG_INFO("Starting backup client...");
    LOG_INFO("Connecting to {}:{}", config.serverHost, config.serverPort);

    // Create and initialize client
    Client client(config);
    
    ErrorCode result = client.initialize();
    if (result != ErrorCode::SUCCESS) {
        LOG_CRITICAL("Failed to initialize client: {}", errorCodeToString(result));
        return 1;
    }

    // Run client
    result = client.run();
    
    if (result != ErrorCode::SUCCESS) {
        LOG_ERROR("Client exited with error: {}", errorCodeToString(result));
        return 1;
    }

    LOG_INFO("Client exited normally");
    return 0;
}
