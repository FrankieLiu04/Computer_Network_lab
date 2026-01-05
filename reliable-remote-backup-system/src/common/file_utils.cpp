#include "common/file_utils.h"
#include <fstream>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define stat _stat
#define mkdir(path, mode) _mkdir(path)
#else
#include <dirent.h>
#include <unistd.h>
#endif

namespace backup {
namespace file {

bool fileExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && (st.st_mode & S_IFREG);
}

bool directoryExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && (st.st_mode & S_IFDIR);
}

bool createDirectory(const std::string& path) {
    if (directoryExists(path)) {
        return true;
    }

#ifdef _WIN32
    return _mkdir(path.c_str()) == 0;
#else
    return mkdir(path.c_str(), 0755) == 0;
#endif
}

uint64_t getFileSize(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return 0;
    }
    return static_cast<uint64_t>(st.st_size);
}

std::vector<std::string> listDirectory(const std::string& path) {
    std::vector<std::string> files;

#ifdef _WIN32
    WIN32_FIND_DATAA findData;
    std::string searchPath = path + "\\*";
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        return files;
    }

    do {
        std::string name = findData.cFileName;
        if (name != "." && name != "..") {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                files.push_back(name);
            }
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
#else
    DIR* dir = opendir(path.c_str());
    if (dir == nullptr) {
        return files;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name != "." && name != "..") {
            // Check if it's a regular file
            std::string fullPath = path + "/" + name;
            struct stat st;
            if (stat(fullPath.c_str(), &st) == 0 && (st.st_mode & S_IFREG)) {
                files.push_back(name);
            }
        }
    }

    closedir(dir);
#endif

    return files;
}

bool deleteFile(const std::string& path) {
#ifdef _WIN32
    return DeleteFileA(path.c_str()) != 0;
#else
    return unlink(path.c_str()) == 0;
#endif
}

bool renameFile(const std::string& oldPath, const std::string& newPath) {
    return rename(oldPath.c_str(), newPath.c_str()) == 0;
}

std::optional<std::string> readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }

    std::string content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );
    return content;
}

bool writeFile(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    file.write(content.data(), content.size());
    return file.good();
}

int64_t readFileToBuffer(const std::string& path, char* buffer, size_t maxSize) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return -1;
    }

    file.read(buffer, maxSize);
    return file.gcount();
}

bool writeBufferToFile(const std::string& path, const char* buffer, size_t size) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    file.write(buffer, size);
    return file.good();
}

}  // namespace file
}  // namespace backup
