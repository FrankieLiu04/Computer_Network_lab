#ifndef BACKUP_COMMON_FILE_UTILS_H
#define BACKUP_COMMON_FILE_UTILS_H

#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace backup {
namespace file {

/**
 * Check if a file exists.
 * @param path Path to the file
 * @return true if file exists, false otherwise
 */
bool fileExists(const std::string& path);

/**
 * Check if a directory exists.
 * @param path Path to the directory
 * @return true if directory exists, false otherwise
 */
bool directoryExists(const std::string& path);

/**
 * Create a directory, including parent directories if needed.
 * @param path Path to the directory
 * @return true if successful or directory already exists, false otherwise
 */
bool createDirectory(const std::string& path);

/**
 * Get the size of a file.
 * @param path Path to the file
 * @return File size in bytes, or 0 if file doesn't exist
 */
uint64_t getFileSize(const std::string& path);

/**
 * Get list of files in a directory.
 * @param path Path to the directory
 * @return Vector of filenames (not full paths), empty if directory doesn't exist
 */
std::vector<std::string> listDirectory(const std::string& path);

/**
 * Delete a file.
 * @param path Path to the file
 * @return true if successful, false otherwise
 */
bool deleteFile(const std::string& path);

/**
 * Rename/move a file.
 * @param oldPath Current path
 * @param newPath New path
 * @return true if successful, false otherwise
 */
bool renameFile(const std::string& oldPath, const std::string& newPath);

/**
 * Read entire file content into a string.
 * @param path Path to the file
 * @return File content, or nullopt if error
 */
std::optional<std::string> readFile(const std::string& path);

/**
 * Write string content to a file.
 * @param path Path to the file
 * @param content Content to write
 * @return true if successful, false otherwise
 */
bool writeFile(const std::string& path, const std::string& content);

/**
 * Read file content into a buffer.
 * @param path Path to the file
 * @param buffer Buffer to read into
 * @param maxSize Maximum bytes to read
 * @return Number of bytes read, or -1 on error
 */
int64_t readFileToBuffer(const std::string& path, char* buffer, size_t maxSize);

/**
 * Write buffer content to a file.
 * @param path Path to the file
 * @param buffer Buffer to write from
 * @param size Number of bytes to write
 * @return true if successful, false otherwise
 */
bool writeBufferToFile(const std::string& path, const char* buffer, size_t size);

}  // namespace file
}  // namespace backup

#endif  // BACKUP_COMMON_FILE_UTILS_H
