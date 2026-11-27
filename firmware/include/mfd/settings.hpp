#pragma once
#include "mfd/record_format.hpp"

namespace mfd {
enum class TemperatureUnit { Celsius, Fahrenheit };
enum class DistanceUnit { Metric, Imperial };
enum class DisplayMode { Telemetry, Navigation, Hybrid };

struct Settings {
    TemperatureUnit temperature = TemperatureUnit::Celsius;
    DistanceUnit distance = DistanceUnit::Metric;
    DisplayMode mode = DisplayMode::Telemetry;
};

inline bool operator==(Settings a, Settings b) {
    return a.temperature == b.temperature && a.distance == b.distance && a.mode == b.mode;
}

class SettingsStorage {
public:
    virtual ~SettingsStorage() = default;
    virtual std::optional<std::string> read() = 0;
    virtual bool replace(std::string_view data) = 0;
};

class SettingsModel {
public:
    explicit SettingsModel(SettingsStorage &storage) : storage_(storage) {
        const auto bytes = storage_.read();
        if (!bytes || bytes->empty() || bytes->back() != '\n' || bytes->size() > record_format::max_line) return;
        const auto payload = record_format::checked_payload(std::string_view(*bytes).substr(0, bytes->size() - 1));
        std::array<std::string_view, 4> fields{};
        unsigned temperature = 0, distance = 0, mode = 0;
        if (payload && record_format::split(*payload, fields) == fields.size() && fields[0] == "1" &&
            record_format::parse(fields[1], temperature) && temperature <= 1 &&
            record_format::parse(fields[2], distance) && distance <= 1 &&
            record_format::parse(fields[3], mode) && mode <= 2) {
            restored_ = true;
            settings_ = {static_cast<TemperatureUnit>(temperature), static_cast<DistanceUnit>(distance),
                         static_cast<DisplayMode>(mode)};
        }
    }

    Settings get() const { return settings_; }
    bool restored() const { return restored_; }

    bool save(Settings next) {
        const auto temperature = static_cast<unsigned>(next.temperature);
        const auto distance = static_cast<unsigned>(next.distance);
        const auto mode = static_cast<unsigned>(next.mode);
        if (temperature > 1 || distance > 1 || mode > 2) return false;
        if (next == settings_) return true;
        const auto bytes = record_format::frame("1," + std::to_string(temperature) + ',' +
                                                std::to_string(distance) + ',' + std::to_string(mode));
        if (!storage_.replace(bytes)) return false;
        settings_ = next;
        return true;
    }

private:
    SettingsStorage &storage_;
    Settings settings_;
    bool restored_ = false;
};
} // namespace mfd
