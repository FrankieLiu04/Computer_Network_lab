#include <gtest/gtest.h>
#include "protocol/message.h"
#include <cstring>

using namespace backup::protocol;

// =============================================================================
// Protocol Constants Tests
// =============================================================================

TEST(ProtocolConstantsTest, BufferSizes) {
    EXPECT_EQ(FILE_NAME_LEN, 128);
    EXPECT_EQ(DATA_BUF_LEN, 3000);
}

// =============================================================================
// Command Message Tests
// =============================================================================

class CmdMsgTest : public ::testing::Test {
protected:
    CmdMsg msg;
    
    void SetUp() override {
        // CmdMsg should be zero-initialized by default constructor
    }
};

TEST_F(CmdMsgTest, DefaultConstruction) {
    EXPECT_EQ(msg.cmd, 0);
    EXPECT_EQ(msg.size, 0);
    EXPECT_EQ(msg.port, 0);
    EXPECT_EQ(msg.error, 0);
    EXPECT_EQ(msg.filename[0], '\0');
}

TEST_F(CmdMsgTest, SetCommand) {
    msg.setCommand(Command::LS);
    EXPECT_EQ(msg.cmd, 1);
    EXPECT_EQ(msg.getCommand(), Command::LS);
    
    msg.setCommand(Command::SEND);
    EXPECT_EQ(msg.getCommand(), Command::SEND);
    
    msg.setCommand(Command::SHUTDOWN);
    EXPECT_EQ(msg.getCommand(), Command::SHUTDOWN);
}

TEST_F(CmdMsgTest, SetFilename) {
    msg.setFilename("test.txt");
    EXPECT_STREQ(msg.filename, "test.txt");
    
    // Test truncation for long filenames
    std::string longName(200, 'a');
    msg.setFilename(longName.c_str());
    EXPECT_EQ(strlen(msg.filename), FILE_NAME_LEN - 1);
    EXPECT_EQ(msg.filename[FILE_NAME_LEN - 1], '\0');
}

TEST_F(CmdMsgTest, StructurePacking) {
    // Verify structure size matches expected packed size
    // cmd(1) + filename(128) + size(4) + port(2) + error(2) = 137
    EXPECT_EQ(sizeof(CmdMsg), 1 + FILE_NAME_LEN + 4 + 2 + 2);
}

// =============================================================================
// Data Message Tests
// =============================================================================

class DataMsgTest : public ::testing::Test {
protected:
    DataMsg msg;
};

TEST_F(DataMsgTest, DefaultConstruction) {
    // Data should be zero-initialized
    EXPECT_EQ(msg.data[0], '\0');
}

TEST_F(DataMsgTest, SetData) {
    const char* testData = "Hello, World!";
    msg.setData(testData, strlen(testData));
    EXPECT_STREQ(msg.data, testData);
}

TEST_F(DataMsgTest, SetDataTruncation) {
    // Test that large data is truncated
    std::string largeData(DATA_BUF_LEN + 100, 'x');
    msg.setData(largeData.c_str(), largeData.size());
    
    // Should be truncated to DATA_BUF_LEN - 1
    EXPECT_LE(strlen(msg.data), DATA_BUF_LEN);
}

TEST_F(DataMsgTest, StructurePacking) {
    EXPECT_EQ(sizeof(DataMsg), DATA_BUF_LEN);
}

// =============================================================================
// Command String Conversion Tests
// =============================================================================

TEST(CommandToStringTest, AllCommands) {
    EXPECT_STREQ(commandToString(Command::NONE), "NONE");
    EXPECT_STREQ(commandToString(Command::LS), "CMD_LS");
    EXPECT_STREQ(commandToString(Command::SEND), "CMD_SEND");
    EXPECT_STREQ(commandToString(Command::GET), "CMD_GET");
    EXPECT_STREQ(commandToString(Command::REMOVE), "CMD_REMOVE");
    EXPECT_STREQ(commandToString(Command::RENAME), "CMD_RENAME");
    EXPECT_STREQ(commandToString(Command::SHUTDOWN), "CMD_SHUTDOWN");
    EXPECT_STREQ(commandToString(Command::ACK), "CMD_ACK");
    EXPECT_STREQ(commandToString(Command::ACK_PORT), "CMD_ACK_PORT");
    EXPECT_STREQ(commandToString(Command::FILE_EXISTS), "CMD_FILE_EXISTS");
    EXPECT_STREQ(commandToString(Command::OVERWRITE), "CMD_OVERWRITE");
    EXPECT_STREQ(commandToString(Command::READY_FOR_NEW_NAME), "CMD_READY_FOR_NEW_NAME");
}

TEST(CommandToStringTest, UnknownCommand) {
    EXPECT_STREQ(commandToString(static_cast<Command>(255)), "UNKNOWN");
}

// =============================================================================
// Binary Serialization Tests
// =============================================================================

TEST(SerializationTest, CmdMsgBinaryLayout) {
    CmdMsg msg;
    msg.setCommand(Command::SEND);
    msg.setFilename("test.txt");
    msg.size = 12345;
    msg.port = 40000;
    msg.error = 0;
    
    // Cast to bytes and verify layout
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&msg);
    
    // First byte should be command
    EXPECT_EQ(bytes[0], static_cast<uint8_t>(Command::SEND));
    
    // Next FILE_NAME_LEN bytes are filename
    EXPECT_EQ(bytes[1], 't');
    EXPECT_EQ(bytes[2], 'e');
}

TEST(SerializationTest, FileListInDataMsg) {
    DataMsg msg;
    
    // Simulate file list format: null-separated strings
    const char* files[] = {"file1.txt", "file2.doc", "file3.pdf"};
    int offset = 0;
    
    for (const char* file : files) {
        strcpy(msg.data + offset, file);
        offset += strlen(file) + 1;  // Include null terminator
    }
    
    // Verify we can read them back
    const char* ptr = msg.data;
    int fileIndex = 0;
    while (*ptr && fileIndex < 3) {
        EXPECT_STREQ(ptr, files[fileIndex]);
        ptr += strlen(ptr) + 1;
        fileIndex++;
    }
    EXPECT_EQ(fileIndex, 3);
}
