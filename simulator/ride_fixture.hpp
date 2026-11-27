#pragma once
#include "../firmware/host/memory_storage.hpp"
#include "mfd/navigation.hpp"
#include <algorithm>
#include <iostream>

namespace simulator {
inline bool ride_fixture(std::string_view scenario) {
    using namespace mfd;
    constexpr std::array scenarios{"ignition", "start", "running", "stall", "restart", "kill-switch",
                                   "ecu-loss", "ecu-reconnect", "power-loss", "storage-failure",
                                   "storage-recovery", "phone-independent"};
    if (std::find(scenarios.begin(), scenarios.end(), scenario) == scenarios.end()) return false;

    host::MemoryRecordingStorage storage;
    Telemetry telemetry;
    Recorder original(storage, 0);
    std::optional<Recorder> reboot;
    Recorder *recorder = &original;
    bool ecu_connected = true;
    std::uint64_t now = 0;
    const auto sample = [&](double rpm, std::uint64_t time) {
        now = time;
        ecu_connected = true;
        telemetry.ingest(Metric::Oil, {108, Source::Mock, Quality::Simulated, time}, time);
        telemetry.ingest(Metric::Intake, {32, Source::Mock, Quality::Simulated, time}, time);
        telemetry.ingest(Metric::Voltage, {14.2, Source::Mock, Quality::Simulated, time}, time);
        telemetry.ingest(Metric::Rpm, {rpm, Source::Mock, Quality::Simulated, time}, time);
        recorder->tick(now, telemetry, ecu_connected);
    };

    sample(0, 0);
    if (scenario != "ignition") sample(1100, 1000);

    std::string_view recovery = "No previous session";
    std::uint64_t recovered_records = 0;
    if (scenario == "running" || scenario == "phone-independent") {
        Navigation phone;
        phone.connect();
        if (scenario == "phone-independent") phone.disconnect();
        for (std::uint64_t time = 2000; time <= 6000; time += 1000) sample(2750, time);
    } else if (scenario == "stall" || scenario == "restart" || scenario == "kill-switch") {
        sample(0, 2000);
        if (scenario == "restart") sample(1150, 3000);
    } else if (scenario == "ecu-loss" || scenario == "ecu-reconnect") {
        telemetry.disconnect();
        ecu_connected = false;
        now = 2000;
        telemetry.ingest(Metric::Voltage, {12.6, Source::Battery, Quality::Valid, now}, now);
        recorder->tick(now, telemetry, ecu_connected);
        if (scenario == "ecu-reconnect") sample(1200, 3000);
    } else if (scenario == "power-loss" || scenario == "storage-failure" || scenario == "storage-recovery") {
        storage.append_error = scenario == "power-loss" ? StorageResult::IoError : StorageResult::Full;
        storage.partial_bytes = 12;
        sample(1200, 2000);
        storage.append_error = StorageResult::Ok;
        if (scenario == "power-loss") {
            const auto recovered = storage.recover(*original.status().session);
            recovery = recovered.state == RecoveryState::Truncated ? "Truncated tail; previous session interrupted"
                                                                   : "Unexpected recovery result";
            recovered_records = recovered.samples;
            telemetry.disconnect();
            now = 0;
            ecu_connected = false;
            reboot.emplace(storage, now);
            recorder = &*reboot;
            recorder->tick(now, telemetry, ecu_connected);
        } else if (scenario == "storage-recovery") {
            sample(1300, 3000);
        }
    }

    const auto rpm = telemetry.get(Metric::Rpm, now);
    const auto status = recorder->status();
    std::cout << "{\"rpm\":";
    if (rpm.value) std::cout << *rpm.value;
    else std::cout << "null";
    std::cout << ",\"rpm_quality\":\"" << name(rpm.quality)
              << "\",\"engine\":\"" << name(status.engine)
              << "\",\"recording\":\"" << name(status.state)
              << "\",\"session\":";
    if (status.session) std::cout << *status.session;
    else std::cout << "null";
    std::cout << ",\"elapsed_ms\":" << status.elapsed_ms
              << ",\"records\":" << status.records
              << ",\"sync_requests\":" << status.sync_requests
              << ",\"storage_errors\":" << status.storage_errors
              << ",\"last_error\":\"" << name(status.last_error)
              << "\",\"recovery\":\"" << recovery
              << "\",\"recovered_records\":" << recovered_records << "}\n";
    return true;
}
} // namespace simulator
