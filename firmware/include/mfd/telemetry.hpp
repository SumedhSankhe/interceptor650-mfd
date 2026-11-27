#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

namespace mfd {
enum class Metric { Oil, Intake, Voltage };
enum class Source { None, Mock, Ecu, Adapter, Battery };
enum class Quality { Missing, Valid, Simulated, Unsupported, Error, Stale };
struct Reading {
    std::optional<double> value;
    Source source = Source::None;
    Quality quality = Quality::Missing;
    std::uint64_t acquired_ms = 0;
};
class Telemetry {
public:
    static constexpr std::uint64_t stale_ms = 3000;
    bool ingest(Metric metric, Reading reading, std::uint64_t now) {
        auto &slot = readings_.at(static_cast<unsigned>(metric));
        if (reading.acquired_ms > now || reading.acquired_ms < slot.acquired_ms ||
            reading.quality == Quality::Stale || reading.quality == Quality::Missing) return false;
        const bool numeric = reading.quality == Quality::Valid || reading.quality == Quality::Simulated;
        if (numeric != reading.value.has_value()) return false;
        if (numeric && (!std::isfinite(*reading.value) || reading.source == Source::None ||
            ((reading.source == Source::Mock) != (reading.quality == Quality::Simulated)))) return false;
        if (metric != Metric::Voltage && numeric && reading.source != Source::Mock && reading.source != Source::Ecu) return false;
        if (numeric && *reading.value < (metric == Metric::Voltage ? 0.0 : -273.15)) return false;
        slot = reading;
        return true;
    }
    Reading get(Metric metric, std::uint64_t now) const {
        auto r = readings_.at(static_cast<unsigned>(metric));
        if (r.value && (now < r.acquired_ms || now - r.acquired_ms >= stale_ms)) {
            r.value.reset(); r.quality = Quality::Stale;
        }
        return r;
    }
    void disconnect() { readings_ = {}; }
private:
    std::array<Reading, 3> readings_{};
};
inline double fahrenheit(double celsius) { return celsius * 1.8 + 32.0; }
} // namespace mfd
