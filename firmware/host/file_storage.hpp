#pragma once
#include "mfd/recording.hpp"
#include "mfd/settings.hpp"
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace mfd::host {
namespace detail {
inline StorageResult error(std::error_code code) {
    if (code == std::errc::no_such_file_or_directory) return StorageResult::Missing;
    if (code == std::errc::no_space_on_device || code == std::errc::file_too_large) return StorageResult::Full;
    if (code == std::errc::permission_denied || code == std::errc::read_only_file_system ||
        code == std::errc::not_a_directory) return StorageResult::Unavailable;
    return StorageResult::IoError;
}

inline StorageResult io_error(int code) {
    return error({code, std::generic_category()});
}

inline std::FILE *open_file(const std::filesystem::path &path) {
#ifdef _WIN32
    std::FILE *file = nullptr;
    const auto result = ::_wfopen_s(&file, path.c_str(), L"r+b");
    if (result) errno = result;
    return file;
#else
    return std::fopen(path.c_str(), "r+b");
#endif
}

inline StorageResult sync_file(const std::filesystem::path &path) {
    auto *file = open_file(path);
    if (!file) return io_error(errno);
#ifdef _WIN32
    const auto result = ::_commit(::_fileno(file));
#else
    const auto result = ::fsync(::fileno(file));
#endif
    const auto status = result == 0 ? StorageResult::Ok : io_error(errno);
    const auto closed = std::fclose(file);
    return status != StorageResult::Ok || closed == 0 ? status : io_error(errno);
}

inline StorageResult append_file(const std::filesystem::path &path, std::string_view bytes) {
    auto *file = open_file(path);
    if (!file) return io_error(errno);
#ifdef _WIN32
    const auto seek = ::_fseeki64(file, 0, SEEK_END);
#else
    const auto seek = ::fseeko(file, 0, SEEK_END);
#endif
    const auto count = seek == 0 ? std::fwrite(bytes.data(), 1, bytes.size(), file) : 0;
    const auto status = count == bytes.size() ? StorageResult::Ok : io_error(errno);
    // fclose drains the C application buffer. The recorder requests a separate file sync.
    const auto closed = std::fclose(file);
    return status != StorageResult::Ok || closed == 0 ? status : io_error(errno);
}

inline int create_file(const std::filesystem::path &path) {
#ifdef _WIN32
    int fd = -1;
    const auto result = ::_wsopen_s(&fd, path.c_str(), _O_BINARY | _O_WRONLY | _O_CREAT | _O_EXCL,
                                    _SH_DENYNO, _S_IREAD | _S_IWRITE);
    return result ? result : (::_close(fd) == 0 ? 0 : errno);
#else
    const auto fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    return fd < 0 ? errno : (::close(fd) == 0 ? 0 : errno);
#endif
}
} // namespace detail

// A single recorder owns each directory. Scanning and syncing belong on its worker.
class FileRecordingStorage final : public RecordingStorage {
public:
    explicit FileRecordingStorage(std::filesystem::path directory) : directory_(std::move(directory)) {}

    StorageResult reserve(std::uint64_t &id) override {
        std::vector<std::uint64_t> ids;
        if (const auto status = scan(ids); status != StorageResult::Ok) return status;
        std::uint64_t candidate = 0;
        for (const auto existing : ids) {
            if (existing > candidate) candidate = existing;
        }
        while (candidate != std::numeric_limits<std::uint64_t>::max()) {
            ++candidate;
            const auto result = detail::create_file(path(candidate));
            if (result == EEXIST) continue;
            if (result) return detail::io_error(result);
            id = candidate;
            return StorageResult::Ok;
        }
        return StorageResult::Full;
    }

    StorageResult append(std::uint64_t id, std::uint64_t offset, std::string_view bytes) override {
        std::error_code code;
        const auto file = path(id);
        const auto size = std::filesystem::file_size(file, code);
        if (code) return detail::error(code);
        if (size < offset) return StorageResult::IoError;
        if (size > offset) {
            std::filesystem::resize_file(file, offset, code);
            if (code) return detail::error(code);
        }
        return detail::append_file(file, bytes);
    }

    StorageResult synchronize(std::uint64_t id) override { return detail::sync_file(path(id)); }

    Recovery recover(std::uint64_t id) const {
        std::ifstream file(path(id), std::ios::binary);
        return file ? recover_recording(file, id) : Recovery{RecoveryState::Unavailable};
    }

    StorageResult recover_all(std::vector<std::pair<std::uint64_t, Recovery>> &sessions) const {
        std::vector<std::uint64_t> ids;
        const auto result = scan(ids);
        if (result == StorageResult::Ok) {
            for (const auto id : ids) sessions.emplace_back(id, recover(id));
        }
        return result;
    }

private:
    std::filesystem::path path(std::uint64_t id) const { return directory_ / (std::to_string(id) + ".csv"); }

    StorageResult scan(std::vector<std::uint64_t> &ids) const {
        std::error_code code;
        for (std::filesystem::directory_iterator it(directory_, code), end; !code && it != end; it.increment(code)) {
            const auto file = it->path();
            std::uint64_t id = 0;
            if (file.extension() == ".csv" && record_format::parse(file.stem().string(), id)) ids.push_back(id);
        }
        return code ? detail::error(code) : StorageResult::Ok;
    }

    std::filesystem::path directory_;
};

class FileSettingsStorage final : public SettingsStorage {
public:
    explicit FileSettingsStorage(std::filesystem::path path) : path_(std::move(path)) {}

    std::optional<std::string> read() override {
        std::ifstream input(path_, std::ios::binary);
        if (!input) return {};
        std::array<char, record_format::max_line + 1> bytes{};
        input.read(bytes.data(), bytes.size());
        if (input.bad() || input.gcount() == static_cast<std::streamsize>(bytes.size())) return {};
        return std::string(bytes.data(), static_cast<std::size_t>(input.gcount()));
    }

    // Same-directory replacement requires a single settings writer.
    bool replace(std::string_view bytes) override {
        auto temp = path_;
        temp += ".tmp";
        std::error_code code;
        std::filesystem::remove(temp, code);
        if (code || detail::create_file(temp)) return false;
        if (detail::append_file(temp, bytes) == StorageResult::Ok &&
            detail::sync_file(temp) == StorageResult::Ok) {
#ifdef _WIN32
            if (::MoveFileExW(temp.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
#else
            std::filesystem::rename(temp, path_, code);
            if (!code) return true;
#endif
        }
        std::filesystem::remove(temp, code);
        return false;
    }

private:
    std::filesystem::path path_;
};
} // namespace mfd::host
