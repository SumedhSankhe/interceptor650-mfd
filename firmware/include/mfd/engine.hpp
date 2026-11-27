#pragma once
#include "mfd/telemetry.hpp"

namespace mfd {
enum class EngineState { Unknown, Running, Stopped };

inline EngineState engine_state(const Telemetry &telemetry, bool ecu_connected, std::uint64_t now) {
    if (!ecu_connected) return EngineState::Unknown;
    const auto rpm = telemetry.get(Metric::Rpm, now);
    if (!rpm.value || (rpm.quality != Quality::Valid && rpm.quality != Quality::Simulated)) {
        return EngineState::Unknown;
    }
    return *rpm.value > 0 ? EngineState::Running : EngineState::Stopped;
}
} // namespace mfd
