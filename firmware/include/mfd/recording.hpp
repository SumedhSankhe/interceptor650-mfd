#pragma once
#include "mfd/record_format.hpp"

namespace mfd {
enum class StorageResult { Ok, Missing, Unavailable, Full, IoError };

inline std::string_view name(StorageResult result) {
    constexpr std::array names{"ok", "missing", "unavailable", "full", "io-error"};
    return names.at(static_cast<unsigned>(result));
}

class RecordingStorage {
public:
    virtual ~RecordingStorage() = default;
    virtual StorageResult reserve(std::uint64_t &id) = 0;
    // Offset is the acknowledged prefix length. Retry replaces only an unacknowledged tail.
    virtual StorageResult append(std::uint64_t id, std::uint64_t offset, std::string_view bytes) = 0;
    virtual StorageResult synchronize(std::uint64_t id) = 0;
};

enum class RecordingState { Pending, Recording, Degraded, Completed };

inline std::string_view name(RecordingState state) {
    constexpr std::array names{"pending", "recording", "degraded", "completed"};
    return names.at(static_cast<unsigned>(state));
}

struct RecordingIntervals {
    std::uint64_t sample_ms = 1000;
    std::uint64_t sync_ms = 5000;
};

struct RecordingStatus {
    std::optional<std::uint64_t> session;
    RecordingState state = RecordingState::Pending;
    EngineState engine = EngineState::Unknown;
    std::uint64_t elapsed_ms = 0;
    std::uint64_t records = 0;
    std::uint64_t sync_requests = 0;
    std::uint64_t storage_errors = 0;
    StorageResult last_error = StorageResult::Ok;
};

// One instance per boot. Invoke on the recording worker, outside acquisition and rendering.
class Recorder {
public:
    Recorder(RecordingStorage &storage, std::uint64_t boot_ms, RecordingIntervals intervals = {})
        : storage_(storage), boot_ms_(boot_ms), intervals_(intervals),
          last_tick_(boot_ms), last_sync_(boot_ms) {}

    const RecordingStatus &status() const { return status_; }

    void tick(std::uint64_t now, const Telemetry &telemetry, bool ecu_connected, Timestamp timestamp = {}) {
        if (finishing_ || now < last_tick_) return;
        update_clock(now);
        const auto engine = engine_state(telemetry, ecu_connected, now);
        const bool stopped = status_.engine == EngineState::Running && engine == EngineState::Stopped;
        status_.engine = engine;

        if (stopped || !last_sample_ || now - *last_sample_ >= intervals_.sample_ms) {
            last_sample_ = now;
            append_failed_ = !prepare() || !append(record_format::sample(
                *status_.session, status_.elapsed_ms, timestamp, engine, ecu_connected,
                status_.storage_errors, telemetry, now));
            if (!append_failed_) {
                ++status_.records;
                if (!sync_failed_) status_.state = RecordingState::Recording;
            }
        }
        if (stopped || now - last_sync_ >= intervals_.sync_ms) synchronize(now);
    }

    // Explicit close is available to a host/operator. Ignition loss has no close callback.
    bool finish(std::uint64_t now) {
        if (status_.state == RecordingState::Completed) return true;
        if (now < last_tick_) return false;
        if (!finishing_) {
            update_clock(now);
            finishing_ = true;
        }
        if (!end_written_) {
            if (!prepare() || !append(record_format::frame(record_format::prefix(*status_.session, 'E') +
                                                          ',' + std::to_string(status_.elapsed_ms)))) return false;
            end_written_ = true;
        }
        if (!synchronize(now)) return false;
        status_.state = RecordingState::Completed;
        return true;
    }

private:
    void update_clock(std::uint64_t now) {
        last_tick_ = now;
        status_.elapsed_ms = now - boot_ms_;
    }

    bool check(StorageResult result) {
        if (result == StorageResult::Ok) return true;
        status_.state = RecordingState::Degraded;
        ++status_.storage_errors;
        status_.last_error = result;
        return false;
    }

    bool prepare() {
        if (!status_.session) {
            std::uint64_t id = 0;
            if (!check(storage_.reserve(id))) return false;
            status_.session = id;
        }
        return offset_ != 0 || append(record_format::frame(record_format::prefix(*status_.session, 'B')));
    }

    bool append(const std::string &record) {
        if (!check(storage_.append(*status_.session, offset_, record))) return false;
        offset_ += record.size();
        return true;
    }

    bool synchronize(std::uint64_t now) {
        last_sync_ = now;
        ++status_.sync_requests;
        if (!status_.session) return false;
        sync_failed_ = !check(storage_.synchronize(*status_.session));
        if (!sync_failed_ && status_.state == RecordingState::Degraded && !append_failed_) {
            status_.state = RecordingState::Recording;
        }
        return !sync_failed_;
    }

    RecordingStorage &storage_;
    const std::uint64_t boot_ms_;
    RecordingIntervals intervals_;
    RecordingStatus status_;
    std::uint64_t last_tick_;
    std::uint64_t last_sync_;
    std::optional<std::uint64_t> last_sample_;
    std::uint64_t offset_ = 0;
    bool sync_failed_ = false;
    bool append_failed_ = false;
    bool finishing_ = false;
    bool end_written_ = false;
};
} // namespace mfd
