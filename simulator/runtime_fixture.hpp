#pragma once
#include "mfd/application.hpp"
#include "../firmware/host/memory_storage.hpp"
#include <cctype>
#include <iomanip>
#include <iostream>

namespace simulator {
using namespace mfd;

inline bool parse_number(std::string_view text, std::uint32_t &value) {
    return record_format::parse(text, value);
}

struct NavigationFixture {
    unsigned state;
    unsigned maneuver;
    std::uint32_t distance;
    unsigned exit_number;
    std::uint32_t age;
    unsigned connection;
};

inline NavigationEvent packet(std::uint64_t link, unsigned session, unsigned sequence,
                              NavState state, unsigned maneuver = 3, std::uint32_t distance = 400,
                              unsigned exit_number = 0) {
    NavigationEvent event{{}, navigation_frame_size, link};
    auto &frame = event.bytes;
    const auto put = [&frame](std::size_t offset, std::uint32_t value) {
        for (unsigned i = 0; i < 4; ++i) frame[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
    };
    frame[0] = 1;
    frame[1] = static_cast<std::uint8_t>(state);
    put(4, session);
    put(8, sequence);
    if (state == NavState::Active) {
        frame[2] = static_cast<std::uint8_t>(maneuver);
        frame[3] = 1;
        put(12, distance);
        if (maneuver == static_cast<unsigned>(Maneuver::Roundabout) && exit_number) {
            frame[3] |= 2;
            frame[16] = static_cast<std::uint8_t>(exit_number);
        }
    }
    return event;
}

class RuntimeFixture {
public:
    explicit RuntimeFixture(Settings settings) {
        SettingsModel model(settings_);
        model.save(settings);
        boot(0);
    }

    ApplicationSnapshot snapshot() const { return app_->latest(); }

    bool replay(std::string_view scene) {
        constexpr std::array scenarios{"ignition", "start", "running", "stall", "restart", "kill-switch",
                                       "ecu-loss", "ecu-reconnect", "power-loss", "storage-failure",
                                       "storage-recovery", "phone-independent"};
        if (std::find(scenarios.begin(), scenarios.end(), scene) == scenarios.end()) return false;

        measure(0, 0);
        if (scene != "ignition") measure(1100, 1000);
        if (scene == "running" || scene == "phone-independent") {
            for (std::uint64_t time = 2000; time <= 6000; time += 1000) measure(2750, time);
        } else if (scene == "stall" || scene == "restart" || scene == "kill-switch") {
            measure(0, 2000);
            if (scene == "restart") measure(1150, 3000);
        } else if (scene == "ecu-loss" || scene == "ecu-reconnect") {
            lose_ecu(2000);
            if (scene == "ecu-reconnect") measure(1200, 3000);
        } else if (scene == "power-loss" || scene == "storage-failure" || scene == "storage-recovery") {
            storage_.append_error = scene == "power-loss" ? StorageResult::IoError : StorageResult::Full;
            storage_.partial_bytes = 12;
            measure(1200, 2000);
            storage_.append_error = StorageResult::Ok;
            if (scene == "power-loss") {
                boot(3000);
                finish_step(3000);
            } else if (scene == "storage-recovery") {
                measure(1300, 3000);
            }
        }
        return true;
    }

    bool apply_fixtures(std::string_view telemetry, NavigationFixture nav, bool phone_absent) {
        if (telemetry != "normal" && telemetry != "stale" && telemetry != "unsupported" &&
            telemetry != "disconnected") return false;
        if (nav.state > static_cast<unsigned>(NavState::Stale) || nav.maneuver < 1 ||
            nav.maneuver > 9 || nav.exit_number > 255 || nav.connection > 2) return false;

        const auto before = snapshot();
        const auto state = static_cast<NavState>(nav.state);
        const std::uint64_t age = state == NavState::Stale ? Navigation::stale_ms : nav.age;
        const auto elapsed = std::max(age, telemetry == "stale" ? Telemetry::stale_ms : 0u);
        const auto final_time = now_ + elapsed;
        const auto received = final_time - age;
        if (state != NavState::Disconnected && !phone_absent) {
            auto link = app_->set_phone(true, received);
            if (nav.connection) {
                if (!app_->ingest(packet(link, 1, 1, NavState::Active), received)) return false;
                app_->set_phone(false, received);
                link = app_->set_phone(true, received);
            }
            if (nav.connection != 1 && !app_->ingest(packet(link, nav.connection == 2 ? 2 : 1, 1,
                state == NavState::Stale ? NavState::Active : state, nav.maneuver, nav.distance,
                nav.exit_number), received)) return false;
        }
        if (telemetry == "disconnected") {
            app_->set_ecu(false, final_time);
        } else if (telemetry != "stale") {
            for (unsigned i = 0; i < before.telemetry.size(); ++i) {
                auto reading = before.telemetry[i];
                const auto metric = static_cast<Metric>(i);
                if (telemetry == "unsupported" && metric == Metric::Oil && before.ecu_connected) {
                    reading = {std::nullopt, Source::Mock, Quality::Unsupported, final_time};
                }
                if (reading.quality == Quality::Missing || reading.quality == Quality::Stale) continue;
                reading.acquired_ms = final_time;
                const bool independent = reading.source == Source::Battery || reading.source == Source::Adapter;
                app_->ingest({metric, reading, independent ? 0 : ecu_link_}, final_time);
            }
        }
        finish_step(final_time);
        return true;
    }

    bool timeline(unsigned step) {
        if (step > 17) return false;
        for (unsigned i = 0; i <= step; ++i) {
            const std::uint64_t time = std::uint64_t{i} * 1000;
            if (i == 17) {
                storage_.append_error = StorageResult::Ok;
                boot(time);
                finish_step(time);
                continue;
            }
            if (i == 7 || i == 14) {
                if (i == 7) guide(time, 5, 2);
                if (i == 14) {
                    storage_.append_error = StorageResult::Full;
                    storage_.partial_bytes = 13;
                }
                lose_ecu(time);
                continue;
            }
            if (i == 8) app_->set_phone(false, time);
            if (i == 3 || i == 4 || i == 5 || i == 6) guide(time, i == 3 ? 3 : 5, i == 3 ? 0 : 2);
            if (i == 15) storage_.append_error = StorageResult::Ok;
            if (i == 16) {
                storage_.append_error = StorageResult::IoError;
                storage_.partial_bytes = 13;
            }
            const bool stopped = i == 0 || i == 5 || i == 13;
            measure(stopped ? 0 : i < 2 ? 1200 : 2600, time, 95 + i, stopped ? 12.6 : 14.1,
                    i < 9 || i > 13);
        }
        return true;
    }

private:
    void boot(std::uint64_t time) {
        // The reducer publishes unavailable live state before archive inspection.
        SettingsModel model(settings_);
        app_.emplace(time, model.get(), model.restored() ? SettingsRecovery::Restored : SettingsRecovery::Defaults);
        RecoverySummary summary;
        summary.pending = false;
        for (const auto &entry : storage_.sessions) summary.include(entry.first, storage_.recover(entry.first));
        app_->accept_recovery(summary);
        worker_.emplace(storage_, time);
        ecu_link_ = 0;
        phone_link_ = nav_sequence_ = 0;
        now_ = time;
    }

    void finish_step(std::uint64_t time, bool drain = true) {
        now_ = time;
        app_->tick(time);
        if (drain) {
            while (auto work = app_->take_recording()) worker_->consume(*work);
            app_->accept_storage(worker_->status());
        }
        app_->publish();
    }

    void measure(double rpm, std::uint64_t time, double oil = 108, double voltage = 14.2, bool drain = true) {
        ecu_link_ = app_->set_ecu(true, time);
        const auto insert = [&](Metric metric, double value) {
            app_->ingest({metric, {value, Source::Mock, Quality::Simulated, time}, ecu_link_}, time);
        };
        insert(Metric::Oil, oil);
        insert(Metric::Intake, 32);
        insert(Metric::Voltage, voltage);
        insert(Metric::Rpm, rpm);
        finish_step(time, drain);
    }

    void lose_ecu(std::uint64_t time) {
        app_->set_ecu(false, time);
        ecu_link_ = 0;
        app_->ingest({Metric::Voltage, {12.6, Source::Battery, Quality::Valid, time}, 0}, time);
        finish_step(time);
    }

    void guide(std::uint64_t time, unsigned maneuver, unsigned exit_number) {
        phone_link_ = app_->set_phone(true, time);
        app_->ingest(packet(phone_link_, 1, ++nav_sequence_, NavState::Active, maneuver, 400, exit_number), time);
    }

    host::MemoryRecordingStorage storage_;
    host::MemorySettingsStorage settings_;
    std::optional<Application> app_;
    std::optional<Recorder> worker_;
    std::uint64_t now_ = 0;
    std::uint64_t ecu_link_ = 0;
    std::uint64_t phone_link_ = 0;
    unsigned nav_sequence_ = 0;
};

inline std::string_view recovery_text(const RecoverySummary &recovery) {
    if (recovery.pending) return "Archive inspection pending";
    if (recovery.result != StorageResult::Ok) return "Archive unavailable";
    if (!recovery.latest_state) return "No previous session";
    constexpr std::array states{"Empty previous session", "Previous session interrupted",
        "Truncated tail; previous session interrupted", "Corrupt record; valid prefix retained",
        "Previous session completed", "Previous session unavailable"};
    return states.at(static_cast<unsigned>(*recovery.latest_state));
}

template <typename Number>
void write_number(const std::optional<Number> &value) {
    if (value) std::cout << +*value;
    else std::cout << "null";
}

inline void write_snapshot(const ApplicationSnapshot &snapshot) {
    const auto &nav = snapshot.navigation;
    const auto &status = snapshot.recording;
    const auto &rpm = snapshot.telemetry[3];
    std::cout << std::setprecision(std::numeric_limits<double>::max_digits10);
    std::cout << "{\"mode\":" << static_cast<unsigned>(snapshot.preferences.mode)
              << ",\"temperature_unit\":" << static_cast<unsigned>(snapshot.preferences.temperature)
              << ",\"distance_unit\":" << static_cast<unsigned>(snapshot.preferences.distance)
              << ",\"telemetry\":[";
    for (unsigned i = 0; i < 3; ++i) {
        const auto &reading = snapshot.telemetry[i];
        if (i) std::cout << ',';
        std::cout << "{\"value\":";
        if (reading.value && i != 2 && snapshot.preferences.temperature == TemperatureUnit::Fahrenheit) {
            std::cout << fahrenheit(*reading.value);
        } else {
            write_number(reading.value);
        }
        const auto quality = name(reading.quality);
        std::cout << ",\"quality\":\"";
        for (const unsigned char ch : quality) std::cout << static_cast<char>(std::toupper(ch));
        const auto source = reading.source == Source::Mock ? "MOCK" :
            reading.source == Source::Battery ? "BATTERY" : "NONE";
        std::cout << "\",\"source\":\"" << source << "\"}";
    }
    std::cout << "],\"navigation\":{\"state\":" << static_cast<unsigned>(nav.state)
              << ",\"maneuver\":" << static_cast<unsigned>(nav.maneuver) << ",\"distance_m\":";
    write_number(nav.distance_m);
    std::cout << ",\"exit_number\":";
    write_number(nav.exit_number);
    std::cout << "},\"diagnostics\":{\"rpm\":";
    write_number(rpm.value);
    std::cout << ",\"rpm_quality\":\"" << name(rpm.quality)
              << "\",\"engine\":\"" << name(snapshot.engine)
              << "\",\"recording\":\"" << name(status.state) << "\",\"session\":";
    write_number(status.session);
    std::cout << ",\"elapsed_ms\":" << snapshot.uptime_ms
              << ",\"records\":" << status.records
              << ",\"sync_requests\":" << snapshot.queue.sync_requests
              << ",\"storage_errors\":" << status.storage_errors
              << ",\"last_error\":\"" << name(status.last_error)
              << "\",\"recovery\":\"" << recovery_text(snapshot.recovery)
              << "\",\"recovered_records\":" << snapshot.recovery.recovered_samples
              << ",\"sequence\":" << snapshot.sequence
              << ",\"created_ms\":" << snapshot.created_ms
              << ",\"ecu\":\"" << (snapshot.ecu_connected ? "connected" : "disconnected")
              << "\",\"phone\":\"" << (snapshot.phone_connected ? "connected" : "disconnected")
              << "\",\"telemetry_age_ms\":";
    const auto &oil = snapshot.telemetry[0];
    if (oil.quality == Quality::Missing) std::cout << "null";
    else std::cout << snapshot.created_ms - oil.acquired_ms;
    std::cout << ",\"navigation_age_ms\":";
    if (nav.sequence) std::cout << snapshot.created_ms - nav.received_ms;
    else std::cout << "null";
    std::cout << ",\"queue_depth\":" << snapshot.queue.depth
              << ",\"queue_peak\":" << snapshot.queue.high_water
              << ",\"dropped_samples\":" << snapshot.queue.dropped_samples
              << ",\"sync_completed_attempts\":" << status.sync_requests
              << ",\"sync_coalesced\":" << snapshot.queue.coalesced_syncs
              << ",\"settings\":\"" << name(snapshot.settings_recovery)
              << "\",\"clock\":\"" << (snapshot.timestamp.quality == ClockQuality::Unknown ? "unknown" :
                  snapshot.timestamp.quality == ClockQuality::Trusted ? "trusted" : "unverified")
              << "\",\"oil_c\":";
    write_number(oil.value);
    std::cout << ",\"voltage_v\":";
    write_number(snapshot.telemetry[2].value);
    std::cout << "}}\n";
}
} // namespace simulator
