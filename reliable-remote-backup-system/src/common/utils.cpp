#include "common/utils.h"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

namespace backup {
namespace utils {

// =============================================================================
// Filename validation
// =============================================================================

bool isValidFilename(const std::string& filename) {
    return getFilenameValidationError(filename).empty();
}

std::string getFilenameValidationError(const std::string& filename) {
    // Check empty
    if (filename.empty()) {
        return "Filename cannot be empty";
    }

    // Check length
    if (filename.length() > 255) {
        return "Filename too long (max 255 characters)";
    }

    // Check for path traversal attempts
    if (filename.find("..") != std::string::npos) {
        return "Path traversal attempt detected (..)";
    }

    // Check for absolute paths (Unix and Windows)
    if (filename[0] == '/' || filename[0] == '\\') {
        return "Absolute paths are not allowed";
    }
    if (filename.length() >= 2 && filename[1] == ':') {
        return "Windows absolute paths are not allowed";
    }

    // Check for path separators
    if (filename.find('/') != std::string::npos || 
        filename.find('\\') != std::string::npos) {
        return "Path separators are not allowed in filename";
    }

    // Check for null bytes
    if (filename.find('\0') != std::string::npos) {
        return "Null bytes are not allowed";
    }

    // Check for invalid characters (Windows-specific)
    const std::string invalidChars = "<>:\"|?*";
    for (char c : filename) {
        if (invalidChars.find(c) != std::string::npos) {
            return "Invalid character in filename: " + std::string(1, c);
        }
        // Control characters
        if (static_cast<unsigned char>(c) < 32) {
            return "Control characters are not allowed";
        }
    }

    // Reserved Windows filenames
    std::string lowerName = toLower(filename);
    // Remove extension if present
    size_t dotPos = lowerName.find('.');
    if (dotPos != std::string::npos) {
        lowerName = lowerName.substr(0, dotPos);
    }
    
    const std::vector<std::string> reserved = {
        "con", "prn", "aux", "nul",
        "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
        "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9"
    };
    for (const auto& r : reserved) {
        if (lowerName == r) {
            return "Reserved filename: " + filename;
        }
    }

    return "";  // Valid
}

std::string sanitizeFilename(const std::string& filename) {
    std::string result;
    result.reserve(filename.length());

    const std::string invalidChars = "<>:\"|?*\\/";
    
    for (char c : filename) {
        if (invalidChars.find(c) != std::string::npos) {
            result += '_';
        } else if (static_cast<unsigned char>(c) < 32) {
            // Skip control characters
            continue;
        } else {
            result += c;
        }
    }

    // Remove leading/trailing spaces and dots
    while (!result.empty() && (result.front() == ' ' || result.front() == '.')) {
        result.erase(result.begin());
    }
    while (!result.empty() && (result.back() == ' ' || result.back() == '.')) {
        result.pop_back();
    }

    if (result.empty()) {
        result = "unnamed";
    }

    return result;
}

// =============================================================================
// Path utilities
// =============================================================================

std::string joinPath(const std::string& base, const std::string& child) {
    if (base.empty()) return child;
    if (child.empty()) return base;

    char lastChar = base.back();
    bool hasTrailingSep = (lastChar == '/' || lastChar == '\\');
    bool hasLeadingSep = (child.front() == '/' || child.front() == '\\');

    if (hasTrailingSep && hasLeadingSep) {
        return base + child.substr(1);
    } else if (!hasTrailingSep && !hasLeadingSep) {
#ifdef _WIN32
        return base + "\\" + child;
#else
        return base + "/" + child;
#endif
    } else {
        return base + child;
    }
}

std::string getFilename(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) {
        return path;
    }
    return path.substr(pos + 1);
}

std::string getDirectory(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) {
        return "";
    }
    return path.substr(0, pos);
}

// =============================================================================
// String utilities
// =============================================================================

std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r\f\v");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(" \t\n\r\f\v");
    return str.substr(start, end - start + 1);
}

std::string toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    
    // Handle empty string case - should return vector with one empty string
    if (str.empty()) {
        tokens.push_back("");
        return tokens;
    }
    
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

// =============================================================================
// Time utilities
// =============================================================================

int64_t getCurrentTimeMs() {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    // Convert from 100-ns intervals since 1601 to ms since epoch
    return static_cast<int64_t>((uli.QuadPart - 116444736000000000ULL) / 10000);
#else
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return static_cast<int64_t>(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
#endif
}

std::string getFormattedTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_buf);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

// =============================================================================
// Size formatting
// =============================================================================

std::string formatBytes(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unitIndex < 4) {
        size /= 1024.0;
        unitIndex++;
    }

    std::ostringstream oss;
    if (unitIndex == 0) {
        oss << bytes << " " << units[unitIndex];
    } else {
        oss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];
    }
    return oss.str();
}

// =============================================================================
// Request ID generation
// =============================================================================

std::string generateRequestId() {
    static std::atomic<uint32_t> counter{0};
    
    // Combine timestamp + counter for uniqueness
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    uint32_t cnt = counter.fetch_add(1, std::memory_order_relaxed);
    
    // Mix bits: lower 16 bits of timestamp + counter
    uint32_t mixed = ((ms & 0xFFFF) << 16) | (cnt & 0xFFFF);
    
    // Format as 8-char hex
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(8) << mixed;
    return oss.str();
}

}  // namespace utils
}  // namespace backup
