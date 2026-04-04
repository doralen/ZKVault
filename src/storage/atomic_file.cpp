#include "storage/atomic_file.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

void WriteAll(int fd, std::string_view contents) {
    std::size_t written = 0;
    while (written < contents.size()) {
        const ssize_t chunk = ::write(
            fd,
            contents.data() + written,
            contents.size() - written);
        if (chunk <= 0) {
            throw std::runtime_error("failed to write temporary file");
        }
        written += static_cast<std::size_t>(chunk);
    }
}

void CloseFile(int& fd) {
    if (fd < 0) {
        return;
    }

    if (::close(fd) != 0) {
        fd = -1;
        throw std::runtime_error("failed to close temporary file");
    }

    fd = -1;
}

void SyncDirectory(const std::filesystem::path& directory) {
    const std::string directory_string = directory.string();
    const int fd = ::open(directory_string.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        throw std::runtime_error("failed to open parent directory");
    }

    if (::fsync(fd) != 0) {
        ::close(fd);
        throw std::runtime_error("failed to flush parent directory");
    }

    if (::close(fd) != 0) {
        throw std::runtime_error("failed to close parent directory");
    }
}

}  // namespace

void WriteFileAtomically(
    const std::filesystem::path& path,
    const std::string& contents) {
    const std::filesystem::path directory =
        path.has_parent_path() ? path.parent_path() : std::filesystem::path(".");
    std::filesystem::create_directories(directory);

    std::string temp_template =
        (directory / (path.filename().string() + ".tmp.XXXXXX")).string();
    std::vector<char> temp_buffer(temp_template.begin(), temp_template.end());
    temp_buffer.push_back('\0');

    int fd = ::mkstemp(temp_buffer.data());
    if (fd < 0) {
        throw std::runtime_error("failed to create temporary file");
    }

    const std::filesystem::path temp_path(temp_buffer.data());
    bool keep_temp_file = true;

    try {
        if (::fchmod(fd, S_IRUSR | S_IWUSR) != 0) {
            throw std::runtime_error("failed to set secure file permissions");
        }

        WriteAll(fd, contents);

        if (::fsync(fd) != 0) {
            throw std::runtime_error("failed to flush temporary file");
        }

        CloseFile(fd);
        std::filesystem::rename(temp_path, path);
        keep_temp_file = false;
        SyncDirectory(directory);
    } catch (...) {
        if (fd >= 0) {
            ::close(fd);
        }

        if (keep_temp_file) {
            std::error_code remove_error;
            std::filesystem::remove(temp_path, remove_error);
        }

        throw;
    }
}
