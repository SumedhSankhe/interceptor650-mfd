#include "mfd/navigation.hpp"
#include "../host/file_storage.hpp"
#include "../host/memory_storage.hpp"
#include <cassert>
#include <iostream>

using namespace mfd;
using namespace mfd::host;

namespace {
void rpm(Telemetry &telemetry, double value, std::uint64_t now, Source source = Source::Mock) {
    assert(telemetry.ingest(Metric::Rpm, {value, source, source == Source::Mock ? Quality::Simulated : Quality::Valid,
                                        now}, now));
}

Recovery inspect(std::string bytes, std::uint64_t id) {
    std::istringstream input(std::move(bytes));
    return recover_recording(input, id);
}

void test_rpm() {
    Telemetry telemetry;
    assert(engine_state(telemetry, true, 0) == EngineState::Unknown);
    for (const auto invalid : {-1.0, std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN()}) {
        assert(!telemetry.ingest(Metric::Rpm, {invalid, Source::Ecu, Quality::Valid, 0}, 0));
    }
    assert(!telemetry.ingest(Metric::Rpm, {450, Source::Battery, Quality::Valid, 0}, 0));
    rpm(telemetry, 0, 0, Source::Ecu);
    assert(telemetry.get(Metric::Rpm, 0).value == 0);
    assert(engine_state(telemetry, true, 0) == EngineState::Stopped);
    assert(engine_state(telemetry, false, 0) == EngineState::Unknown);
    assert(engine_state(telemetry, true, 2999) == EngineState::Stopped);
    const auto stale = telemetry.get(Metric::Rpm, 3000);
    assert(!stale.value && stale.quality == Quality::Stale);
    assert(engine_state(telemetry, true, 3000) == EngineState::Unknown);
    for (const auto quality : {Quality::Unsupported, Quality::Error}) {
        assert(telemetry.ingest(Metric::Rpm, {std::nullopt, Source::Ecu, quality, 3001}, 3001));
        assert(engine_state(telemetry, true, 3001) == EngineState::Unknown);
    }
    rpm(telemetry, 1234.25, 3002);
    assert(engine_state(telemetry, true, 3002) == EngineState::Running);
    assert(telemetry.get(Metric::Rpm, 3002).quality == Quality::Simulated);
}

void test_lifecycle() {
    MemoryRecordingStorage storage;
    Recorder recorder(storage, 100);
    Telemetry telemetry;
    Navigation phone;
    phone.connect();
    const auto observe = [&](double value, std::uint64_t now) {
        rpm(telemetry, value, now);
        recorder.tick(now, telemetry, true);
    };
    observe(0, 100);
    assert(recorder.status().session == std::uint64_t{1} && recorder.status().records == 1);
    assert(recorder.status().sync_requests == 0 && recorder.status().engine == EngineState::Stopped);
    observe(1500, 1100);
    observe(0, 1150); // Stall, observed between periodic samples.
    assert(recorder.status().sync_requests == 1 && recorder.status().records == 3);
    phone.disconnect();
    observe(800, 2150);
    observe(0, 3150); // Kill switch looks identical to a stall.
    assert(recorder.status().sync_requests == 2 && storage.sync_calls == 2);
    observe(900, 4150);
    telemetry.disconnect();
    assert(telemetry.ingest(Metric::Voltage, {12.6, Source::Battery, Quality::Valid, 5150}, 5150));
    recorder.tick(5150, telemetry, false, {std::int64_t{1700000000000}, ClockQuality::Trusted});
    assert(recorder.status().engine == EngineState::Unknown);
    assert(recorder.status().sync_requests == 2);
    observe(1100, 6150);
    observe(1100, 8149);
    assert(recorder.status().sync_requests == 2);
    observe(1100, 8150); // Periodic boundary from most recent stop sync.
    assert(recorder.status().sync_requests == 3);
    assert(recorder.status().engine == EngineState::Running);
    assert(recorder.status().session == std::uint64_t{1} && storage.sessions.size() == 1);
    assert(recorder.status().elapsed_ms == 8050 && recorder.status().records == 9);
    const auto &bytes = storage.sessions.at(1);
    assert(bytes.find(",1700000000000,trusted,unknown,0,0,,missing,,missing,") != std::string::npos);
    assert(bytes.find(",valid,battery,,missing,") != std::string::npos);
    assert(storage.recover(1).state == RecoveryState::Interrupted);
    assert(storage.recover(1).samples == recorder.status().records);
    assert(recorder.finish(8151));
    assert(recorder.status().state == RecordingState::Completed);
    assert(storage.recover(1).state == RecoveryState::Completed);
}

void test_disconnect_masks_ecu_readings() {
    MemoryRecordingStorage storage;
    Telemetry telemetry;
    Recorder recorder(storage, 0, {100, 500});
    rpm(telemetry, 1500, 0, Source::Ecu);
    assert(telemetry.ingest(Metric::Oil, {108, Source::Ecu, Quality::Valid, 0}, 0));
    assert(telemetry.ingest(Metric::Voltage, {12.6, Source::Battery, Quality::Valid, 0}, 0));
    recorder.tick(0, telemetry, true);
    recorder.tick(100, telemetry, false);
    assert(recorder.status().engine == EngineState::Unknown && recorder.status().sync_requests == 0);
    assert(storage.sessions.at(1).find(",unknown,0,0,,missing,,missing,") != std::string::npos);
    assert(storage.sessions.at(1).find(",valid,battery,,missing,") != std::string::npos);
    rpm(telemetry, 0, 200, Source::Ecu);
    recorder.tick(200, telemetry, true);
    assert(recorder.status().engine == EngineState::Stopped && recorder.status().sync_requests == 0);
}

void test_failures_and_truncation() {
    assert(record_format::crc32("123456789") == 0xcbf43926u);
    MemoryRecordingStorage storage;
    Telemetry telemetry;
    Recorder recorder(storage, 0, {10, 50});
    storage.reserve_error = StorageResult::Missing;
    recorder.tick(0, telemetry, false);
    assert(!recorder.status().session && recorder.status().last_error == StorageResult::Missing);
    storage.reserve_error = StorageResult::Unavailable;
    recorder.tick(10, telemetry, false);
    assert(recorder.status().last_error == StorageResult::Unavailable);
    storage.reserve_error = StorageResult::Ok;
    recorder.tick(20, telemetry, false);
    assert(recorder.status().state == RecordingState::Recording);
    assert(recorder.status().storage_errors == 2);
    const auto good = storage.sessions.at(1);
    const auto header = good.find('\n') + 1;
    assert(inspect("", 1).state == RecoveryState::Empty);
    assert(inspect(good.substr(0, header), 1).state == RecoveryState::Interrupted);
    // Every cut between header and sample boundaries recovers the acknowledged prefix.
    for (std::size_t cut = 1; cut < good.size(); ++cut) {
        if (cut == header) continue;
        const auto recovered = inspect(good.substr(0, cut), 1);
        assert(recovered.state == RecoveryState::Truncated && recovered.samples == 0);
        assert(recovered.valid_bytes == (cut < header ? 0 : header));
    }
    auto corrupt = good;
    corrupt[header + 2] = '2';
    assert(inspect(corrupt, 1).state == RecoveryState::Corrupt);
    assert(inspect(good, 2).state == RecoveryState::Corrupt);
    assert(inspect(good + record_format::frame("1,1,S,21"), 1).state == RecoveryState::Corrupt);
    assert(inspect(good + record_format::frame("1,1,E,1"), 1).state == RecoveryState::Corrupt);

    storage.append_error = StorageResult::Full;
    storage.partial_bytes = 8;
    recorder.tick(30, telemetry, false);
    assert(recorder.status().last_error == StorageResult::Full && recorder.status().records == 1);
    assert(storage.recover(1).state == RecoveryState::Truncated);
    assert(storage.recover(1).valid_bytes == good.size());
    storage.append_error = StorageResult::Ok;
    recorder.tick(40, telemetry, false);
    assert(recorder.status().state == RecordingState::Recording && storage.recover(1).samples == 2);

    storage.sync_error = StorageResult::IoError;
    recorder.tick(50, telemetry, false);
    assert(recorder.status().state == RecordingState::Degraded && recorder.status().sync_requests == 1);
    recorder.tick(51, telemetry, false);
    assert(recorder.status().sync_requests == 1);
    storage.sync_error = StorageResult::Ok;
    recorder.tick(100, telemetry, false);
    assert(recorder.status().state == RecordingState::Recording && recorder.status().sync_requests == 2);
    assert(storage.sessions.at(1).find(",unknown,0,4,") != std::string::npos);

    storage.sync_error = StorageResult::IoError;
    assert(!recorder.finish(101));
    const auto closing = storage.sessions.at(1);
    assert(inspect(closing.substr(0, closing.size() - 1), 1).state == RecoveryState::Truncated);
    storage.sync_error = StorageResult::Ok;
    assert(recorder.finish(102) && storage.sessions.at(1) == closing);
    assert(inspect(closing + "garbage", 1).state == RecoveryState::Corrupt);
}

void test_settings() {
    MemorySettingsStorage storage;
    const Settings desired{TemperatureUnit::Fahrenheit, DistanceUnit::Imperial, DisplayMode::Hybrid};
    SettingsModel model(storage);
    assert(model.get() == Settings{});
    assert(model.save(Settings{}) && storage.writes == 0);
    assert(model.save(desired) && model.save(desired) && storage.writes == 1);
    assert(SettingsModel(storage).get() == desired);
    storage.fail = true;
    assert(!model.save(Settings{}) && model.get() == desired);
    assert(SettingsModel(storage).get() == desired);
    const auto valid = *storage.bytes;
    for (std::size_t cut = 0; cut < valid.size(); ++cut) {
        storage.bytes = valid.substr(0, cut);
        assert(SettingsModel(storage).get() == Settings{});
    }
    storage.bytes = record_format::frame("2,1,1,2");
    assert(SettingsModel(storage).get() == Settings{});
    storage.bytes = record_format::frame("1,1,1,4");
    assert(SettingsModel(storage).get() == Settings{});
    storage.bytes = valid;
    (*storage.bytes)[2] = '0';
    assert(SettingsModel(storage).get() == Settings{});
}

void test_filesystem() {
    const auto base = std::filesystem::temp_directory_path();
    std::filesystem::path path;
    for (unsigned i = 0; ; ++i) {
        path = base / ("mfd-recording-test-" + std::to_string(i));
        if (std::filesystem::create_directory(path)) break;
    }
    const auto read_bytes = [](const std::filesystem::path &file) {
        std::ifstream stream(file, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(stream), {});
    };

    FileRecordingStorage files(path);
    FileRecordingStorage absent(path / "absent");
    std::uint64_t unused = 0;
    assert(absent.reserve(unused) == StorageResult::Missing);
    Telemetry telemetry;
    {
        Recorder completed(files, 0);
        completed.tick(0, telemetry, false);
        assert(completed.finish(100));
    }
    const auto preserved = read_bytes(path / "1.csv");
    {
        Recorder interrupted(files, 0, {1000, 1000});
        interrupted.tick(0, telemetry, false);
        interrupted.tick(1000, telemetry, false);
        // Inject bytes after the last acknowledged record, then reboot without finish().
        assert(files.append(2, files.recover(2).valid_bytes, "1,2,S,") == StorageResult::Ok);
    }
    const auto partial = read_bytes(path / "2.csv");
    FileRecordingStorage rebooted(path);
    std::vector<std::pair<std::uint64_t, Recovery>> reports;
    assert(rebooted.recover_all(reports) == StorageResult::Ok && reports.size() == 2);
    assert(rebooted.recover(1).state == RecoveryState::Completed);
    assert(rebooted.recover(2).state == RecoveryState::Truncated && rebooted.recover(2).samples == 2);
    Recorder fresh(rebooted, 0);
    fresh.tick(0, telemetry, false);
    assert(fresh.status().session == std::uint64_t{3});
    assert(read_bytes(path / "1.csv") == preserved && read_bytes(path / "2.csv") == partial);

    FileSettingsStorage preferences(path / "settings.csv");
    const Settings desired{TemperatureUnit::Fahrenheit, DistanceUnit::Imperial, DisplayMode::Hybrid};
    {
        SettingsModel settings(preferences);
        assert(settings.save(desired));
        assert(SettingsModel(preferences).get() == desired);
        const Settings newer{TemperatureUnit::Celsius, DistanceUnit::Metric, DisplayMode::Navigation};
        assert(settings.save(newer) && SettingsModel(preferences).get() == newer);
    }
    for (const auto *file : {"1.csv", "2.csv", "3.csv", "settings.csv"}) std::filesystem::remove(path / file);
    assert(std::filesystem::remove(path));
}
} // namespace

int main() {
    test_rpm();
    test_lifecycle();
    test_disconnect_masks_ecu_readings();
    test_failures_and_truncation();
    test_settings();
    test_filesystem();
    std::cout << "recording assertions passed\n";
}
