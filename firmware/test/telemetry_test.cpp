#include "mfd/telemetry.hpp"
#include <cassert>
#include <limits>
#include <iostream>
using namespace mfd;
int main() {
    Telemetry t;
    assert(t.get(Metric::Oil, 0).quality == Quality::Missing);
    assert(t.ingest(Metric::Oil, {108, Source::Mock, Quality::Simulated, 100}, 100));
    assert(t.get(Metric::Oil, 3099).value == 108);
    assert(t.get(Metric::Oil, 3099).quality == Quality::Simulated);
    assert(t.get(Metric::Oil, 3100).quality == Quality::Stale);
    assert(!t.get(Metric::Oil, 3100).value);
    assert(!t.get(Metric::Oil, 99).value);
    assert(!t.ingest(Metric::Oil, {99, Source::Mock, Quality::Valid, 200}, 200));
    assert(!t.ingest(Metric::Oil, {99, Source::Mock, Quality::Simulated, 99}, 200));
    assert(!t.ingest(Metric::Oil, {99, Source::Mock, Quality::Simulated, 201}, 200));
    assert(!t.ingest(Metric::Oil, {std::numeric_limits<double>::quiet_NaN(), Source::Mock, Quality::Simulated, 200}, 200));
    assert(!t.ingest(Metric::Oil, {99, Source::Adapter, Quality::Valid, 200}, 200));
    assert(!t.ingest(Metric::Voltage, {-1, Source::Mock, Quality::Simulated, 200}, 200));
    assert(!t.ingest(Metric::Oil, {-274, Source::Mock, Quality::Simulated, 200}, 200));
    assert(!t.ingest(Metric::Oil, {99, Source::Mock, Quality::Unsupported, 200}, 200));
    assert(!t.ingest(Metric::Oil, {std::nullopt, Source::Mock, Quality::Simulated, 200}, 200));
    assert(t.get(Metric::Oil, 200).value == 108);
    assert(t.ingest(Metric::Voltage, {14.2, Source::Ecu, Quality::Valid, 200}, 200));
    assert(t.get(Metric::Voltage, 200).source == Source::Ecu);
    assert(t.ingest(Metric::Intake, {std::nullopt, Source::Ecu, Quality::Unsupported, 200}, 200));
    assert(t.get(Metric::Intake, 3200).quality == Quality::Unsupported);
    assert(t.ingest(Metric::Oil, {std::nullopt, Source::Mock, Quality::Error, 200}, 200));
    assert(t.get(Metric::Oil, 200).quality == Quality::Error);
    assert(!t.get(Metric::Oil, 200).value);
    assert(fahrenheit(0) == 32 && fahrenheit(100) == 212);
    t.disconnect();
    assert(t.get(Metric::Voltage, 200).quality == Quality::Missing);
    assert(t.ingest(Metric::Voltage, {14, Source::Mock, Quality::Simulated, 201}, 201));
    assert(t.get(Metric::Voltage, 201).value == 14);
    std::cout << "telemetry assertions passed\n";
}
