#ifndef BACKUP_COMMON_UTILS_H
#define BACKUP_COMMON_UTILS_H

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>

// Windows compatibility: Define ssize_t
#ifdef _WIN32
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

namespace backup {
namespace utils {

// =============================================================================
// Filename validation
// =============================================================================

/**
 * Validate a filename for safety.
 * Checks for:
 * - Path traversal attempts (../)
 * - Absolute paths
 * - Invalid characters
 * - Empty or too long filenames
 *
 * @param filename The filename to validate
 * @return true if the filename is safe, false otherwise
 */
bool isValidFilename(const std::string& filename);

/**
 * Get detailed validation error for a filename.
 * @param filename The filename to validate
 * @return Error message, or empty string if valid
 */
std::string getFilenameValidationError(const std::string& filename);

/**
 * Sanitize a filename by removing dangerous characters.
 * Note: This may change the filename, so validation should be preferred.
 * @param filename The filename to sanitize
 * @return Sanitized filename
 */
std::string sanitizeFilename(const std::string& filename);

// =============================================================================
// Path utilities
// =============================================================================

/**
 * Join path components safely.
 * @param base Base path
 * @param child Child path component
 * @return Joined path
 */
std::string joinPath(const std::string& base, const std::string& child);

/**
 * Get the filename from a path.
 * @param path Full path
 * @return Filename component
 */
std::string getFilename(const std::string& path);

/**
 * Get the directory from a path.
 * @param path Full path
 * @return Directory component
 */
std::string getDirectory(const std::string& path);

// =============================================================================
// String utilities
// =============================================================================

/**
 * Trim whitespace from both ends of a string.
 * @param str String to trim
 * @return Trimmed string
 */
std::string trim(const std::string& str);

/**
 * Convert string to lowercase.
 * @param str String to convert
 * @return Lowercase string
 */
std::string toLower(const std::string& str);

/**
 * Split string by delimiter.
 * @param str String to split
 * @param delimiter Delimiter character
 * @return Vector of tokens
 */
std::vector<std::string> split(const std::string& str, char delimiter);

// =============================================================================
// Time utilities
// =============================================================================

/**
 * Get current timestamp in milliseconds.
 * @return Timestamp in milliseconds since epoch
 */
int64_t getCurrentTimeMs();

/**
 * Get formatted timestamp string.
 * @return Formatted timestamp (e.g., "2024-01-05 12:34:56.789")
 */
std::string getFormattedTimestamp();

/**
 * Simple timer for measuring elapsed time.
 */
class Timer {
public:
    Timer() : start_(std::chrono::steady_clock::now()) {}

    void reset() {
        start_ = std::chrono::steady_clock::now();
    }

    int64_t elapsedMs() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_).count();
    }

    double elapsedSeconds() const {
        return elapsedMs() / 1000.0;
    }

private:
    std::chrono::steady_clock::time_point start_;
};

// =============================================================================
// Size formatting
// =============================================================================

/**
 * Format byte size to human-readable string.
 * @param bytes Size in bytes
 * @return Formatted string (e.g., "1.5 MB")
 */
std::string formatBytes(uint64_t bytes);

// =============================================================================
// Request ID generation
// =============================================================================

/**
 * Generate a unique request ID for log correlation.
 * Format: 8-character hex string (e.g., "a1b2c3d4")
 * Thread-safe.
 * @return Unique request ID
 */
std::string generateRequestId();

}  // namespace utils
}  // namespace backup

#endif  // BACKUP_COMMON_UTILS_H
