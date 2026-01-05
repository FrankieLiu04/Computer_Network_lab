#include "server/server.h"
#include "common/logger.h"
#include <iostream>
#include <cstring>
#include <cstdlib>

using namespace backup;
using namespace backup::server;

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [-port <udp_port>] [-backup <dir>] [-log]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -port <udp_port>  UDP port to listen on (default: auto-assign)" << std::endl;
    std::cout << "  -backup <dir>     Backup directory (default: backup)" << std::endl;
    std::cout << "  -log              Enable file logging" << std::endl;
    std::cout << "  -help             Show this help message" << std::endl;
}

int main(int argc, char* argv[]) {
    ServerConfig config;
    bool enableFileLog = false;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "-port") == 0 && i + 1 < argc) {
            config.udpPort = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "-backup") == 0 && i + 1 < argc) {
            config.backupDir = argv[++i];
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

    // Initialize logger
    Logger::init("server", enableFileLog, "logs");
    LOG_INFO("Starting backup server...");

    // Create and initialize server
    Server server(config);
    
    ErrorCode result = server.initialize();
    if (result != ErrorCode::SUCCESS) {
        LOG_CRITICAL("Failed to initialize server: {}", errorCodeToString(result));
        return 1;
    }

    // Run server
    result = server.run();
    
    if (result != ErrorCode::SUCCESS) {
        LOG_ERROR("Server exited with error: {}", errorCodeToString(result));
        return 1;
    }

    LOG_INFO("Server exited normally");
    return 0;
}
