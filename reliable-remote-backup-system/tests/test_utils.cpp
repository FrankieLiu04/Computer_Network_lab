#include <gtest/gtest.h>
#include "common/utils.h"

using namespace backup::utils;

// =============================================================================
// Filename Validation Tests
// =============================================================================

class FilenameValidationTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(FilenameValidationTest, ValidFilenames) {
    EXPECT_TRUE(isValidFilename("test.txt"));
    EXPECT_TRUE(isValidFilename("my_file.doc"));
    EXPECT_TRUE(isValidFilename("file-name.pdf"));
    EXPECT_TRUE(isValidFilename("document.backup.tar.gz"));
    EXPECT_TRUE(isValidFilename("123456789"));
    EXPECT_TRUE(isValidFilename("a"));
}

TEST_F(FilenameValidationTest, EmptyFilename) {
    EXPECT_FALSE(isValidFilename(""));
    EXPECT_EQ(getFilenameValidationError(""), "Filename cannot be empty");
}

TEST_F(FilenameValidationTest, PathTraversalAttempts) {
    EXPECT_FALSE(isValidFilename(".."));
    EXPECT_FALSE(isValidFilename("../"));
    EXPECT_FALSE(isValidFilename("../etc/passwd"));
    EXPECT_FALSE(isValidFilename("foo/../bar"));
    EXPECT_FALSE(isValidFilename("..."));
    
    std::string error = getFilenameValidationError("../secret");
    EXPECT_EQ(error, "Path traversal attempt detected (..)");
}

TEST_F(FilenameValidationTest, AbsolutePaths) {
    EXPECT_FALSE(isValidFilename("/etc/passwd"));
    EXPECT_FALSE(isValidFilename("\\Windows\\System32"));
    EXPECT_FALSE(isValidFilename("C:\\Windows"));
    EXPECT_FALSE(isValidFilename("D:\\file.txt"));
}

TEST_F(FilenameValidationTest, PathSeparators) {
    EXPECT_FALSE(isValidFilename("folder/file.txt"));
    EXPECT_FALSE(isValidFilename("folder\\file.txt"));
    EXPECT_FALSE(isValidFilename("a/b/c.txt"));
}

TEST_F(FilenameValidationTest, InvalidCharacters) {
    EXPECT_FALSE(isValidFilename("file<name>.txt"));
    EXPECT_FALSE(isValidFilename("file:name.txt"));
    EXPECT_FALSE(isValidFilename("file\"name.txt"));
    EXPECT_FALSE(isValidFilename("file|name.txt"));
    EXPECT_FALSE(isValidFilename("file?name.txt"));
    EXPECT_FALSE(isValidFilename("file*name.txt"));
}

TEST_F(FilenameValidationTest, ReservedWindowsNames) {
    EXPECT_FALSE(isValidFilename("CON"));
    EXPECT_FALSE(isValidFilename("con"));
    EXPECT_FALSE(isValidFilename("PRN"));
    EXPECT_FALSE(isValidFilename("AUX"));
    EXPECT_FALSE(isValidFilename("NUL"));
    EXPECT_FALSE(isValidFilename("COM1"));
    EXPECT_FALSE(isValidFilename("LPT1"));
    EXPECT_FALSE(isValidFilename("con.txt"));  // Extension shouldn't matter
    EXPECT_FALSE(isValidFilename("nul.doc"));
}

TEST_F(FilenameValidationTest, TooLongFilename) {
    std::string longName(256, 'a');
    EXPECT_FALSE(isValidFilename(longName));
    
    std::string okName(255, 'a');
    EXPECT_TRUE(isValidFilename(okName));
}

// =============================================================================
// String Utility Tests
// =============================================================================

class StringUtilsTest : public ::testing::Test {};

TEST_F(StringUtilsTest, Trim) {
    EXPECT_EQ(trim("  hello  "), "hello");
    EXPECT_EQ(trim("hello"), "hello");
    EXPECT_EQ(trim("  "), "");
    EXPECT_EQ(trim(""), "");
    EXPECT_EQ(trim("\t\nhello\r\n"), "hello");
}

TEST_F(StringUtilsTest, ToLower) {
    EXPECT_EQ(toLower("HELLO"), "hello");
    EXPECT_EQ(toLower("Hello World"), "hello world");
    EXPECT_EQ(toLower("already lowercase"), "already lowercase");
    EXPECT_EQ(toLower(""), "");
    EXPECT_EQ(toLower("123ABC"), "123abc");
}

TEST_F(StringUtilsTest, Split) {
    auto parts = split("a,b,c", ',');
    ASSERT_EQ(parts.size(), 3);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], "b");
    EXPECT_EQ(parts[2], "c");
    
    parts = split("single", ',');
    ASSERT_EQ(parts.size(), 1);
    EXPECT_EQ(parts[0], "single");
    
    parts = split("", ',');
    ASSERT_EQ(parts.size(), 1);
    EXPECT_EQ(parts[0], "");
}

// =============================================================================
// Path Utility Tests
// =============================================================================

class PathUtilsTest : public ::testing::Test {};

TEST_F(PathUtilsTest, JoinPath) {
    // Basic joining
#ifdef _WIN32
    EXPECT_EQ(joinPath("folder", "file.txt"), "folder\\file.txt");
#else
    EXPECT_EQ(joinPath("folder", "file.txt"), "folder/file.txt");
#endif
    
    // Empty base
    EXPECT_EQ(joinPath("", "file.txt"), "file.txt");
    
    // Empty child
    EXPECT_EQ(joinPath("folder", ""), "folder");
    
    // Trailing separator handling
    EXPECT_EQ(joinPath("folder/", "file.txt"), "folder/file.txt");
    EXPECT_EQ(joinPath("folder", "/file.txt"), "folder/file.txt");
    EXPECT_EQ(joinPath("folder/", "/file.txt"), "folder/file.txt");
}

TEST_F(PathUtilsTest, GetFilename) {
    EXPECT_EQ(getFilename("folder/file.txt"), "file.txt");
    EXPECT_EQ(getFilename("folder\\file.txt"), "file.txt");
    EXPECT_EQ(getFilename("/path/to/file.txt"), "file.txt");
    EXPECT_EQ(getFilename("file.txt"), "file.txt");
    EXPECT_EQ(getFilename(""), "");
}

TEST_F(PathUtilsTest, GetDirectory) {
    EXPECT_EQ(getDirectory("folder/file.txt"), "folder");
    EXPECT_EQ(getDirectory("folder\\file.txt"), "folder");
    EXPECT_EQ(getDirectory("/path/to/file.txt"), "/path/to");
    EXPECT_EQ(getDirectory("file.txt"), "");
    EXPECT_EQ(getDirectory(""), "");
}

// =============================================================================
// Size Formatting Tests
// =============================================================================

class SizeFormattingTest : public ::testing::Test {};

TEST_F(SizeFormattingTest, FormatBytes) {
    EXPECT_EQ(formatBytes(0), "0 B");
    EXPECT_EQ(formatBytes(100), "100 B");
    EXPECT_EQ(formatBytes(1023), "1023 B");
    EXPECT_EQ(formatBytes(1024), "1.00 KB");
    EXPECT_EQ(formatBytes(1536), "1.50 KB");
    EXPECT_EQ(formatBytes(1024 * 1024), "1.00 MB");
    EXPECT_EQ(formatBytes(1024 * 1024 * 1024), "1.00 GB");
}

// =============================================================================
// Timer Tests
// =============================================================================

class TimerTest : public ::testing::Test {};

TEST_F(TimerTest, BasicTiming) {
    Timer timer;
    
    // Just verify it compiles and runs
    int64_t elapsed = timer.elapsedMs();
    EXPECT_GE(elapsed, 0);
    
    double elapsedSec = timer.elapsedSeconds();
    EXPECT_GE(elapsedSec, 0.0);
}

TEST_F(TimerTest, Reset) {
    Timer timer;
    
    // Just verify reset works
    timer.reset();
    int64_t elapsed = timer.elapsedMs();
    EXPECT_GE(elapsed, 0);
}
