#include <gtest/gtest.h>
#include "common/error_codes.h"

using namespace backup;

// =============================================================================
// Error Code Tests
// =============================================================================

TEST(ErrorCodeTest, SuccessCode) {
    EXPECT_EQ(static_cast<uint16_t>(ErrorCode::SUCCESS), 0);
    EXPECT_TRUE(isSuccess(ErrorCode::SUCCESS));
}

TEST(ErrorCodeTest, ErrorCodeRanges) {
    // General errors should be 1-99
    EXPECT_GE(static_cast<uint16_t>(ErrorCode::UNKNOWN_ERROR), 1);
    EXPECT_LT(static_cast<uint16_t>(ErrorCode::CANCELLED), 100);
    
    // File errors should be 100-199
    EXPECT_GE(static_cast<uint16_t>(ErrorCode::FILE_NOT_FOUND), 100);
    EXPECT_LT(static_cast<uint16_t>(ErrorCode::FILE_TOO_LARGE), 200);
    
    // Directory errors should be 200-299
    EXPECT_GE(static_cast<uint16_t>(ErrorCode::DIRECTORY_NOT_FOUND), 200);
    EXPECT_LT(static_cast<uint16_t>(ErrorCode::DIRECTORY_READ_ERROR), 300);
    
    // Network errors should be 300-399
    EXPECT_GE(static_cast<uint16_t>(ErrorCode::SOCKET_CREATE_ERROR), 300);
    EXPECT_LT(static_cast<uint16_t>(ErrorCode::CONNECTION_TIMEOUT), 400);
    
    // Protocol errors should be 400-499
    EXPECT_GE(static_cast<uint16_t>(ErrorCode::INVALID_COMMAND), 400);
    EXPECT_LT(static_cast<uint16_t>(ErrorCode::PROTOCOL_ERROR), 500);
    
    // Server errors should be 500-599
    EXPECT_GE(static_cast<uint16_t>(ErrorCode::SERVER_BUSY), 500);
    EXPECT_LE(static_cast<uint16_t>(ErrorCode::SERVER_SHUTDOWN), 599);
}

TEST(ErrorCodeTest, ErrorCodeToString) {
    EXPECT_STREQ(errorCodeToString(ErrorCode::SUCCESS), "SUCCESS");
    EXPECT_STREQ(errorCodeToString(ErrorCode::FILE_NOT_FOUND), "FILE_NOT_FOUND");
    EXPECT_STREQ(errorCodeToString(ErrorCode::SOCKET_CREATE_ERROR), "SOCKET_CREATE_ERROR");
    EXPECT_STREQ(errorCodeToString(ErrorCode::PATH_TRAVERSAL_ATTEMPT), "PATH_TRAVERSAL_ATTEMPT");
}

TEST(ErrorCodeTest, NetworkConversion) {
    ErrorCode original = ErrorCode::FILE_NOT_FOUND;
    uint16_t networkValue = toNetworkError(original);
    ErrorCode converted = fromNetworkError(networkValue);
    
    EXPECT_EQ(original, converted);
    EXPECT_EQ(networkValue, 100);
}

TEST(ErrorCodeTest, ErrorDescriptions) {
    std::string desc = getErrorDescription(ErrorCode::SUCCESS);
    EXPECT_FALSE(desc.empty());
    EXPECT_NE(desc.find("success"), std::string::npos);
    
    desc = getErrorDescription(ErrorCode::FILE_NOT_FOUND);
    EXPECT_FALSE(desc.empty());
    EXPECT_NE(desc.find("not found"), std::string::npos);
    
    desc = getErrorDescription(ErrorCode::PATH_TRAVERSAL_ATTEMPT);
    EXPECT_FALSE(desc.empty());
    EXPECT_NE(desc.find("traversal"), std::string::npos);
}

TEST(ErrorCodeTest, IsSuccessFunction) {
    EXPECT_TRUE(isSuccess(ErrorCode::SUCCESS));
    EXPECT_FALSE(isSuccess(ErrorCode::UNKNOWN_ERROR));
    EXPECT_FALSE(isSuccess(ErrorCode::FILE_NOT_FOUND));
    EXPECT_FALSE(isSuccess(ErrorCode::SOCKET_CREATE_ERROR));
}

// =============================================================================
// Error Code Coverage Tests
// =============================================================================

TEST(ErrorCodeCoverageTest, AllErrorCodesHaveStrings) {
    // Test that all error codes have a string representation
    std::vector<ErrorCode> codes = {
        ErrorCode::SUCCESS,
        ErrorCode::UNKNOWN_ERROR,
        ErrorCode::INVALID_ARGUMENT,
        ErrorCode::TIMEOUT,
        ErrorCode::CANCELLED,
        ErrorCode::FILE_NOT_FOUND,
        ErrorCode::FILE_EXISTS,
        ErrorCode::FILE_OPEN_ERROR,
        ErrorCode::FILE_READ_ERROR,
        ErrorCode::FILE_WRITE_ERROR,
        ErrorCode::FILE_DELETE_ERROR,
        ErrorCode::FILE_RENAME_ERROR,
        ErrorCode::INVALID_FILENAME,
        ErrorCode::PATH_TRAVERSAL_ATTEMPT,
        ErrorCode::FILE_TOO_LARGE,
        ErrorCode::DIRECTORY_NOT_FOUND,
        ErrorCode::DIRECTORY_CREATE_ERROR,
        ErrorCode::DIRECTORY_READ_ERROR,
        ErrorCode::SOCKET_CREATE_ERROR,
        ErrorCode::SOCKET_BIND_ERROR,
        ErrorCode::SOCKET_LISTEN_ERROR,
        ErrorCode::SOCKET_ACCEPT_ERROR,
        ErrorCode::SOCKET_CONNECT_ERROR,
        ErrorCode::SOCKET_SEND_ERROR,
        ErrorCode::SOCKET_RECV_ERROR,
        ErrorCode::CONNECTION_CLOSED,
        ErrorCode::CONNECTION_TIMEOUT,
        ErrorCode::INVALID_COMMAND,
        ErrorCode::INVALID_MESSAGE,
        ErrorCode::UNEXPECTED_RESPONSE,
        ErrorCode::PROTOCOL_ERROR,
        ErrorCode::SERVER_BUSY,
        ErrorCode::SERVER_SHUTDOWN
    };
    
    for (ErrorCode code : codes) {
        const char* str = errorCodeToString(code);
        EXPECT_NE(str, nullptr) << "Error code " << static_cast<int>(code) << " has no string";
        EXPECT_STRNE(str, "UNKNOWN") << "Error code " << static_cast<int>(code) << " returns UNKNOWN";
    }
}

TEST(ErrorCodeCoverageTest, AllErrorCodesHaveDescriptions) {
    std::vector<ErrorCode> codes = {
        ErrorCode::SUCCESS,
        ErrorCode::FILE_NOT_FOUND,
        ErrorCode::SOCKET_CREATE_ERROR,
        ErrorCode::INVALID_COMMAND,
        ErrorCode::SERVER_SHUTDOWN
    };
    
    for (ErrorCode code : codes) {
        std::string desc = getErrorDescription(code);
        EXPECT_FALSE(desc.empty()) << "Error code " << static_cast<int>(code) << " has no description";
    }
}
