#include "mfd/application.hpp"
#include "../host/memory_storage.hpp"
#include <cassert>
#include <iostream>

using namespace mfd;
using namespace mfd::host;

namespace {
NavigationEvent turn(std::uint64_t link, std::uint8_t session = 1, std::uint8_t sequence = 1) {
    return {{1, 1, 3, 1, session, 0, 0, 0, sequence, 0, 0, 0, 0x90, 1, 0, 0, 0, 0, 0, 0},
            navigation_frame_size, link};
}

bool measure(Application &app, Metric metric, double value, std::uint64_t time,
             std::uint64_t link, Source source = Source::Mock) {
    return app.ingest({metric, {value, source, source == Source::Mock ? Quality::Simulated : Quality::Valid,
                               time}, link}, time);
}

std::size_t drain(Application &app, Recorder &worker) {
    std::size_t count = 0;
    while (auto work = app.take_recording()) {
        worker.consume(*work);
        ++count;
    }
    app.accept_storage(worker.status());
    app.publish();
    return count;
}

void test_source_isolation() {
    Application app(0);
    const auto boot = app.latest();
    assert(boot.recovery.pending && !boot.ecu_connected && !boot.phone_connected);
    assert(!boot.telemetry[0].value && boot.navigation.state == NavState::Disconnected);

    const auto ecu = app.set_ecu(true, 0);
    const auto phone = app.set_phone(true, 0);
    assert(measure(app, Metric::Rpm, 1400, 0, ecu));
    assert(measure(app, Metric::Oil, 107, 0, ecu));
    assert(measure(app, Metric::Voltage, 12.8, 0, 0, Source::Battery));
    assert(app.ingest(turn(phone), 0));
    assert(app.tick(100));
    const auto running = app.latest();
    assert(running.engine == EngineState::Running && running.navigation.distance_m == 400u);
    assert(!app.ingest(turn(phone), 101));
    assert(app.ingest(turn(phone, 1, 3), 102));
    assert(!app.ingest(turn(phone, 1, 2), 103));

    app.set_ecu(false, 104);
    const auto gap = app.latest();
    assert(!gap.telemetry[0].value && !gap.telemetry[3].value && gap.telemetry[2].value == 12.8);
    assert(gap.engine == EngineState::Unknown && gap.navigation.state == NavState::Active);
    assert(running.telemetry[0].value == 107 && boot.sequence < gap.sequence);
    const auto new_ecu = app.set_ecu(true, 105);
    assert(!measure(app, Metric::Oil, 999, 105, ecu));
    assert(!app.ingest({Metric::Oil, {999, Source::Mock, Quality::Simulated, 104}, new_ecu}, 105));
    assert(!app.latest().telemetry[0].value);
    assert(measure(app, Metric::Oil, 109, 106, new_ecu));

    app.set_phone(false, 107);
    assert(app.latest().navigation.state == NavState::Disconnected);
    assert(app.latest().telemetry[0].value == 109);
    const auto new_phone = app.set_phone(true, 108);
    assert(!app.ingest(turn(phone, 2), 108));
    assert(!app.ingest(turn(new_phone), 108));
    assert(app.latest().navigation.state == NavState::Unavailable);
    assert(app.ingest(turn(new_phone, 2), 108));
    app.set_ecu(false, 109);
    app.set_phone(false, 110);
    assert(!app.latest().ecu_connected && !app.latest().phone_connected);
    assert(!app.latest().telemetry[0].value && !app.latest().navigation.distance_m);
    assert(app.latest().rejected_navigation == 4);
}

void test_scheduler_and_backpressure() {
    MemoryRecordingStorage storage;
    Recorder worker(storage, 0);
    Application app(0);
    const auto ecu = app.set_ecu(true, 0);
    assert(measure(app, Metric::Rpm, 2000, 0, ecu));
    assert(app.tick(0));
    assert(drain(app, worker) == 1);
    const auto session = worker.status().session;
    // Leave the worker unscheduled for four samples, then observe a stop while full.
    for (std::uint64_t now = 1000; now <= 4000; now += 1000) {
        assert(measure(app, Metric::Rpm, 2100, now, ecu));
        assert(app.tick(now));
    }
    assert(measure(app, Metric::Rpm, 0, 4500, ecu));
    app.publish();
    const auto stopped = app.latest();
    assert(stopped.engine == EngineState::Stopped && stopped.queue.depth == Application::recording_capacity);
    assert(stopped.queue.dropped_samples == 1 && stopped.queue.sync_requests == 1);
    for (std::uint64_t now : {1000u, 2000u, 3000u, 4000u}) {
        const auto work = app.take_recording();
        assert(work && work->sample && work->sample->created_ms == now);
        assert(work->sample->engine == EngineState::Running);
        assert(work->sync_ms.has_value() == (now == 4000u));
        if (work->sync_ms) assert(*work->sync_ms == 4500u);
        worker.consume(*work);
    }
    assert(storage.sync_calls == 1 && worker.status().session == session);
    assert(measure(app, Metric::Rpm, 1500, 5500, ecu));
    assert(app.tick(5500));
    assert(drain(app, worker) == 1 && app.latest().engine == EngineState::Running);
    assert(worker.status().session == session);

    const auto before = worker.status().records;
    const auto earlier_snapshot = app.latest().sequence;
    assert(app.tick(9000000000ULL));
    assert(drain(app, worker) == 1);
    assert(worker.status().records == before + 1 && worker.status().sync_requests == 2);
    assert(app.latest().engine == EngineState::Unknown && app.latest().telemetry[3].quality == Quality::Stale);
    assert(!app.tick(6000));
    assert(!measure(app, Metric::Rpm, 0, 6000, ecu));
    app.publish();
    assert(app.latest().sequence > earlier_snapshot && app.latest().clock_regressions == 2);

    Application clocks(100);
    assert(clocks.set_clock({-1000, ClockQuality::Unverified}, 100));
    assert(clocks.tick(200));
    assert(clocks.latest().timestamp.unix_ms == -900);
    assert(clocks.latest().timestamp.quality == ClockQuality::Unverified);
    assert(clocks.set_clock({std::numeric_limits<std::int64_t>::max(), ClockQuality::Trusted}, 200));
    assert(clocks.tick(300));
    assert(!clocks.latest().timestamp.unix_ms);
}

void test_coalesced_sync_barriers() {
    Application app(0, {}, SettingsRecovery::Defaults, {100, 500});
    const auto ecu = app.set_ecu(true, 0);
    for (std::uint64_t now : {0u, 100u, 200u, 300u}) {
        assert(measure(app, Metric::Rpm, 1600, now, ecu));
        assert(app.tick(now));
    }
    // No space for the stop sample; the barrier follows the four accepted items.
    assert(measure(app, Metric::Rpm, 0, 400, ecu));
    assert(app.tick(900));
    app.publish();
    assert(app.latest().queue.dropped_samples == 2 && app.latest().queue.coalesced_syncs == 1);
    assert(app.latest().queue.sync_requests == 2 && app.latest().queue.high_water == 4);
    for (unsigned i = 0; i < 3; ++i) {
        const auto work = app.take_recording();
        assert(work && work->sample && !work->sync_ms);
    }
    const auto barrier = app.take_recording();
    assert(barrier && barrier->sample && barrier->sample->created_ms == 300u && barrier->sync_ms == 900u);
    assert(!app.take_recording());
}

void test_boot_storage_and_fault_combinations() {
    MemoryRecordingStorage storage;
    MemorySettingsStorage preferences;
    SettingsModel settings(preferences);
    const Settings expected{TemperatureUnit::Fahrenheit, DistanceUnit::Imperial, DisplayMode::Hybrid};
    assert(settings.save(expected));
    {
        Application app(0);
        Recorder worker(storage, 0);
        storage.reserve_error = StorageResult::Missing;
        assert(app.tick(0));
        assert(drain(app, worker) == 1);
        assert(app.latest().recording.state == RecordingState::Degraded && !app.latest().recording.session);
        assert(app.latest().recording.last_error == StorageResult::Missing);
        storage.reserve_error = StorageResult::Ok;
        const auto ecu = app.set_ecu(true, 1000);
        const auto phone = app.set_phone(true, 1000);
        assert(measure(app, Metric::Rpm, 1750, 1000, ecu));
        assert(app.ingest(turn(phone), 1000));
        assert(app.tick(1000));
        drain(app, worker);
        assert(app.latest().recording.session == std::uint64_t{1});
        storage.append_error = StorageResult::Full;
        storage.partial_bytes = 9;
        storage.sync_error = StorageResult::IoError;
        assert(measure(app, Metric::Rpm, 0, 1100, ecu));
        app.set_phone(false, 1100);
        app.set_ecu(false, 1100);
        drain(app, worker);
        const auto degraded = app.latest();
        assert(degraded.engine == EngineState::Unknown && degraded.navigation.state == NavState::Disconnected);
        assert(degraded.recording.storage_errors == 3 && degraded.recording.sync_requests == 1);
        assert(degraded.queue.sync_requests == 1);
        // Abrupt cutoff deliberately omits finish().
    }
    const auto previous = storage.sessions.at(1);
    const SettingsModel recovered_settings(preferences);
    Application reboot(0, recovered_settings.get(), SettingsRecovery::Restored);
    assert(reboot.latest().recovery.pending && reboot.latest().preferences == expected);
    assert(!reboot.latest().telemetry[3].value && reboot.latest().navigation.sequence == 0);
    RecoverySummary summary;
    summary.pending = false;
    summary.include(1, storage.recover(1));
    reboot.accept_recovery(summary);
    storage.append_error = storage.sync_error = StorageResult::Ok;
    Recorder new_worker(storage, 0);
    assert(reboot.tick(0));
    drain(reboot, new_worker);
    assert(recovered_settings.restored() && reboot.latest().recording.session == std::uint64_t{2});
    assert(reboot.latest().recovery.latest_state == RecoveryState::Truncated);
    assert(reboot.latest().recovery.recovered_samples == 1 && storage.sessions.at(1) == previous);
}
} // namespace

int main() {
    test_source_isolation();
    test_scheduler_and_backpressure();
    test_coalesced_sync_barriers();
    test_boot_storage_and_fault_combinations();
    std::cout << "application assertions passed\n";
}
