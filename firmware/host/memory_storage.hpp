#pragma once
#include "mfd/recording.hpp"
#include "mfd/settings.hpp"
#include <map>

namespace mfd::host {
class MemoryRecordingStorage final : public RecordingStorage {
public:
    StorageResult reserve_error = StorageResult::Ok;
    StorageResult append_error = StorageResult::Ok;
    StorageResult sync_error = StorageResult::Ok;
    std::size_t partial_bytes = 0;
    std::map<std::uint64_t, std::string> sessions;
    std::uint64_t sync_calls = 0;

    StorageResult reserve(std::uint64_t &id) override {
        if (reserve_error != StorageResult::Ok) return reserve_error;
        id = sessions.empty() ? 1 : sessions.rbegin()->first + 1;
        sessions.try_emplace(id);
        return StorageResult::Ok;
    }

    StorageResult append(std::uint64_t id, std::uint64_t offset, std::string_view bytes) override {
        const auto file = sessions.find(id);
        if (file == sessions.end() || file->second.size() < offset) return StorageResult::IoError;
        file->second.resize(static_cast<std::size_t>(offset));
        file->second.append(append_error == StorageResult::Ok ? bytes : bytes.substr(0, partial_bytes));
        return append_error;
    }

    StorageResult synchronize(std::uint64_t) override {
        ++sync_calls;
        return sync_error;
    }

    Recovery recover(std::uint64_t id) const {
        const auto file = sessions.find(id);
        if (file == sessions.end()) return {RecoveryState::Unavailable};
        std::istringstream input(file->second);
        return recover_recording(input, id);
    }
};

class MemorySettingsStorage final : public SettingsStorage {
public:
    std::optional<std::string> bytes;
    bool fail = false;
    unsigned writes = 0;

    std::optional<std::string> read() override { return bytes; }

    bool replace(std::string_view data) override {
        ++writes;
        if (fail) return false;
        bytes = std::string(data);
        return true;
    }
};
} // namespace mfd::host
