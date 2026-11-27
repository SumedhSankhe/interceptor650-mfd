#pragma once
#include "mfd/navigation.hpp"
#include "mfd/recording.hpp"
#include "mfd/settings.hpp"
#include <algorithm>

namespace mfd {
enum class SettingsRecovery { Defaults, Restored, Saved, SaveFailed };
inline std::string_view name(SettingsRecovery status) {
    constexpr std::array names{"defaults", "restored", "saved", "save-failed"};
    return names.at(static_cast<unsigned>(status));
}

struct RecoverySummary {
    bool pending = true;
    StorageResult result = StorageResult::Ok;
    std::array<std::uint64_t, 6> sessions{};
    std::uint64_t recovered_samples = 0;
    std::optional<RecoveryState> latest_state;
    std::optional<std::uint64_t> latest_session;

    void include(std::uint64_t id, Recovery recovery) {
        ++sessions.at(static_cast<unsigned>(recovery.state));
        recovered_samples += recovery.samples;
        if (!latest_session || id > *latest_session) {
            latest_state = recovery.state;
            latest_session = id;
        }
    }
};

struct QueueStatus {
    std::size_t depth = 0;
    std::size_t high_water = 0;
    std::uint64_t dropped_samples = 0;
    std::uint64_t sync_requests = 0;
    std::uint64_t coalesced_syncs = 0;
};

struct ApplicationSnapshot {
    std::uint64_t sequence = 0;
    std::uint64_t created_ms = 0;
    std::uint64_t uptime_ms = 0;
    TelemetryReadings telemetry{};
    bool ecu_connected = false;
    EngineState engine = EngineState::Unknown;
    NavigationSnapshot navigation{};
    bool phone_connected = false;
    Settings preferences{};
    SettingsRecovery settings_recovery = SettingsRecovery::Defaults;
    Timestamp timestamp{};
    RecordingStatus recording{};
    RecoverySummary recovery{};
    QueueStatus queue{};
    std::uint64_t clock_regressions = 0;
    std::uint64_t rejected_navigation = 0;
};

struct TelemetryEvent {
    Metric metric;
    Reading reading;
    std::uint64_t ecu_link;
};

struct NavigationEvent {
    std::array<std::uint8_t, navigation_frame_size> bytes;
    std::size_t size;
    std::uint64_t phone_link;
};

// Single-owner reducer. Adapters transfer values across their task boundary; this class owns no I/O or locks.
class Application {
public:
    static constexpr std::size_t recording_capacity = 4;
    static constexpr std::uint64_t display_interval_ms = 100;

    explicit Application(std::uint64_t boot_ms, Settings preferences = {},
                         SettingsRecovery settings = SettingsRecovery::Defaults,
                         RecordingIntervals intervals = {})
        : boot_ms_(boot_ms), now_(boot_ms), last_sync_(boot_ms), preferences_(preferences),
          settings_(settings), intervals_(intervals) {
        publish();
    }

    std::uint64_t set_ecu(bool connected, std::uint64_t now) {
        if (!advance(now)) return 0;
        if (connected != ecu_connected_) {
            ecu_connected_ = connected;
            if (connected) {
                ++ecu_generation_;
                ecu_since_ = now_;
            } else {
                telemetry_.disconnect_ecu();
                engine_ = EngineState::Unknown;
            }
            publish();
        }
        return connected ? ecu_generation_ : 0;
    }

    std::uint64_t set_phone(bool connected, std::uint64_t now) {
        if (!advance(now)) return 0;
        if (connected != phone_connected_) {
            phone_connected_ = connected;
            if (connected) {
                ++phone_generation_;
                navigation_.connect();
            } else {
                navigation_.disconnect();
            }
            publish();
        }
        return connected ? phone_generation_ : 0;
    }

    bool ingest(TelemetryEvent event, std::uint64_t now) {
        if (!advance(now)) return false;
        const auto index = static_cast<unsigned>(event.metric);
        if (index >= latest_.telemetry.size()) return false;
        const bool independent_voltage = event.metric == Metric::Voltage &&
            (event.reading.source == Source::Battery || event.reading.source == Source::Adapter);
        if (independent_voltage ? event.ecu_link != 0 :
            (!ecu_connected_ || event.ecu_link != ecu_generation_ || event.reading.acquired_ms < ecu_since_)) return false;
        if (!telemetry_.ingest(event.metric, event.reading, now_)) return false;
        const auto next = engine_state(telemetry_, ecu_connected_, now_);
        const bool stopped = engine_ == EngineState::Running && next == EngineState::Stopped;
        engine_ = next;
        if (stopped) schedule(true);
        return true;
    }

    bool ingest(NavigationEvent event, std::uint64_t now) {
        if (!advance(now)) return false;
        if (!phone_connected_ || event.phone_link != phone_generation_ ||
            event.size > event.bytes.size() || !navigation_.ingest(event.bytes.data(), event.size, now_)) {
            ++rejected_navigation_;
            return false;
        }
        return true;
    }

    bool set_clock(Timestamp clock, std::uint64_t now) {
        if (!advance(now)) return false;
        clock_ = clock;
        clock_anchor_ms_ = now_;
        return true;
    }

    bool apply_settings(Settings preferences, SettingsRecovery result, std::uint64_t now) {
        if (!advance(now)) return false;
        if (result != SettingsRecovery::SaveFailed) preferences_ = preferences;
        settings_ = result;
        publish();
        return true;
    }

    void accept_storage(RecordingStatus status) { recording_ = status; }
    void accept_recovery(RecoverySummary summary) { recovery_ = summary; }

    bool tick(std::uint64_t now) {
        if (!advance(now)) return false;
        schedule(false);
        if (now_ - latest_.created_ms >= display_interval_ms) publish();
        return true;
    }

    // Immediate publication for boot, link and settings changes, and deterministic host steps.
    void publish() {
        latest_ = {latest_.sequence + 1, now_, now_ - boot_ms_, telemetry_.snapshot(now_),
                   ecu_connected_, engine_, navigation_.get(now_), phone_connected_, preferences_, settings_,
                   clock_at_now(), recording_, recovery_, queue_, clock_regressions_, rejected_navigation_};
    }

    ApplicationSnapshot latest() const { return latest_; }

    std::optional<RecordingWork> take_recording() {
        if (!queue_.depth) return {};
        const auto work = work_[head_];
        head_ = (head_ + 1) % work_.size();
        --queue_.depth;
        return work;
    }

private:
    bool advance(std::uint64_t now) {
        if (now < now_) {
            ++clock_regressions_;
            return false;
        }
        now_ = now;
        // Expire the old RPM before classifying an incoming zero as a stop.
        engine_ = engine_state(telemetry_, ecu_connected_, now_);
        return true;
    }

    Timestamp clock_at_now() const {
        if (!clock_.unix_ms || clock_.quality == ClockQuality::Unknown) return {};
        const auto delta = now_ - clock_anchor_ms_;
        constexpr auto limit = std::numeric_limits<std::int64_t>::max();
        if (delta > static_cast<std::uint64_t>(limit)) return {};
        const auto offset = static_cast<std::int64_t>(delta);
        if (*clock_.unix_ms > limit - offset) return {};
        return {*clock_.unix_ms + offset, clock_.quality};
    }

    void schedule(bool stopped) {
        const bool sync = stopped || now_ - last_sync_ >= intervals_.sync_ms;
        const bool sample = stopped || !last_sample_ || now_ - *last_sample_ >= intervals_.sample_ms;
        if (sample) {
            last_sample_ = now_;
            if (queue_.depth == work_.size()) {
                ++queue_.dropped_samples;
            } else {
                push({RecordingSample{now_, telemetry_.snapshot(now_), engine_, ecu_connected_, clock_at_now()}, {}});
            }
        }
        if (sync) {
            last_sync_ = now_;
            ++queue_.sync_requests;
            if (!queue_.depth) push({});
            auto &tail = work_[(head_ + queue_.depth - 1) % work_.size()];
            if (tail.sync_ms) ++queue_.coalesced_syncs;
            tail.sync_ms = now_;
        }
    }

    void push(RecordingWork work) {
        work_[(head_ + queue_.depth) % work_.size()] = work;
        ++queue_.depth;
        queue_.high_water = std::max(queue_.high_water, queue_.depth);
    }

    const std::uint64_t boot_ms_;
    std::uint64_t now_;
    std::uint64_t last_sync_;
    std::uint64_t ecu_generation_ = 0;
    std::uint64_t ecu_since_ = 0;
    std::uint64_t phone_generation_ = 0;
    bool ecu_connected_ = false;
    bool phone_connected_ = false;
    Telemetry telemetry_;
    Navigation navigation_;
    EngineState engine_ = EngineState::Unknown;
    Settings preferences_;
    SettingsRecovery settings_;
    RecordingIntervals intervals_;
    Timestamp clock_{};
    std::uint64_t clock_anchor_ms_ = 0;
    RecordingStatus recording_{};
    RecoverySummary recovery_{};
    ApplicationSnapshot latest_{};
    std::array<RecordingWork, recording_capacity> work_{};
    std::size_t head_ = 0;
    QueueStatus queue_{};
    std::optional<std::uint64_t> last_sample_;
    std::uint64_t clock_regressions_ = 0;
    std::uint64_t rejected_navigation_ = 0;
};
} // namespace mfd
