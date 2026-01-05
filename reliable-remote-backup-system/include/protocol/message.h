#ifndef BACKUP_PROTOCOL_MESSAGE_H
#define BACKUP_PROTOCOL_MESSAGE_H

#include <cstdint>
#include <cstring>

namespace backup {
namespace protocol {

// Protocol constants
constexpr size_t FILE_NAME_LEN = 128;
constexpr size_t DATA_BUF_LEN = 3000;

// Command types
enum class Command : uint8_t {
    NONE = 0,
    LS = 1,
    SEND = 2,
    GET = 3,
    REMOVE = 4,
    RENAME = 5,
    SHUTDOWN = 6,
    ACK = 7,
    ACK_PORT = 8,
    FILE_EXISTS = 9,
    OVERWRITE = 10,
    READY_FOR_NEW_NAME = 11  // New: handshake for rename
};

// Convert command to string for logging
inline const char* commandToString(Command cmd) {
    switch (cmd) {
        case Command::NONE: return "NONE";
        case Command::LS: return "CMD_LS";
        case Command::SEND: return "CMD_SEND";
        case Command::GET: return "CMD_GET";
        case Command::REMOVE: return "CMD_REMOVE";
        case Command::RENAME: return "CMD_RENAME";
        case Command::SHUTDOWN: return "CMD_SHUTDOWN";
        case Command::ACK: return "CMD_ACK";
        case Command::ACK_PORT: return "CMD_ACK_PORT";
        case Command::FILE_EXISTS: return "CMD_FILE_EXISTS";
        case Command::OVERWRITE: return "CMD_OVERWRITE";
        case Command::READY_FOR_NEW_NAME: return "CMD_READY_FOR_NEW_NAME";
        default: return "UNKNOWN";
    }
}

// Command message structure (packed for network transmission)
#pragma pack(push, 1)
struct CmdMsg {
    uint8_t cmd;                    // Command type
    char filename[FILE_NAME_LEN];   // Filename
    uint32_t size;                  // File size or port number
    uint16_t port;                  // TCP port number
    uint16_t error;                 // Error code

    CmdMsg() : cmd(0), size(0), port(0), error(0) {
        std::memset(filename, 0, FILE_NAME_LEN);
    }

    void setCommand(Command command) {
        cmd = static_cast<uint8_t>(command);
    }

    Command getCommand() const {
        return static_cast<Command>(cmd);
    }

    void setFilename(const char* name) {
        std::strncpy(filename, name, FILE_NAME_LEN - 1);
        filename[FILE_NAME_LEN - 1] = '\0';
    }

    // Network byte order conversion helpers
    void hostToNetwork();
    void networkToHost();
};

// Data message structure
struct DataMsg {
    char data[DATA_BUF_LEN];

    DataMsg() {
        std::memset(data, 0, DATA_BUF_LEN);
    }

    void setData(const char* src, size_t len) {
        size_t copyLen = (len < DATA_BUF_LEN) ? len : DATA_BUF_LEN - 1;
        std::memcpy(data, src, copyLen);
        if (copyLen < DATA_BUF_LEN) {
            data[copyLen] = '\0';
        }
    }
};
#pragma pack(pop)

// Verify structure sizes at compile time
static_assert(sizeof(CmdMsg) == 1 + FILE_NAME_LEN + 4 + 2 + 2, 
              "CmdMsg size mismatch - check packing");
static_assert(sizeof(DataMsg) == DATA_BUF_LEN, 
              "DataMsg size mismatch");

}  // namespace protocol
}  // namespace backup

#endif  // BACKUP_PROTOCOL_MESSAGE_H
