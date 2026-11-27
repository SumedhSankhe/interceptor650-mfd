#pragma once
#include "mfd/engine.hpp"
#include <charconv>
#include <iomanip>
#include <istream>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>

namespace mfd {
enum class ClockQuality { Unknown, Unverified, Trusted };

struct Timestamp {
    std::optional<std::int64_t> unix_ms;
    ClockQuality quality = ClockQuality::Unknown;
};

inline std::string_view name(Quality quality) {
    constexpr std::array names{"missing", "valid", "simulated", "unsupported", "error", "stale"};
    return names.at(static_cast<unsigned>(quality));
}

inline std::string_view name(Source source) {
    constexpr std::array names{"none", "mock", "ecu", "adapter", "battery"};
    return names.at(static_cast<unsigned>(source));
}

inline std::string_view name(EngineState state) {
    constexpr std::array names{"unknown", "running", "stopped"};
    return names.at(static_cast<unsigned>(state));
}

namespace record_format {
inline constexpr std::size_t max_line = 512;

template <typename Number>
bool parse(std::string_view field, Number &number) {
    if (field.empty()) return false;
    const auto end = field.data() + field.size();
    const auto result = std::from_chars(field.data(), end, number);
    return result.ec == std::errc{} && result.ptr == end;
}

inline std::uint32_t crc32(std::string_view bytes) {
    std::uint32_t crc = 0xffffffffu;
    for (const unsigned char byte : bytes) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xedb88320u : 0u);
        }
    }
    return ~crc;
}

inline std::string frame(const std::string &payload) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << payload << ',' << std::hex << std::setfill('0') << std::setw(8) << crc32(payload) << '\n';
    return out.str();
}

inline std::optional<std::string_view> checked_payload(std::string_view line) {
    const auto comma = line.rfind(',');
    if (comma == std::string_view::npos || line.size() - comma != 9) return {};
    const auto digits = line.substr(comma + 1);
    std::uint32_t checksum = 0;
    const auto end = digits.data() + digits.size();
    const auto result = std::from_chars(digits.data(), end, checksum, 16);
    const auto payload = line.substr(0, comma);
    if (result.ec != std::errc{} || result.ptr != end || checksum != crc32(payload)) return {};
    return payload;
}

template <std::size_t N>
std::size_t split(std::string_view payload, std::array<std::string_view, N> &fields) {
    for (std::size_t i = 0; i < N; ++i) {
        const auto comma = payload.find(',');
        fields[i] = payload.substr(0, comma);
        if (comma == std::string_view::npos) return i + 1;
        payload.remove_prefix(comma + 1);
    }
    return 0;
}

inline std::string prefix(std::uint64_t id, char kind) {
    return "1," + std::to_string(id) + ',' + kind;
}

inline std::string sample(std::uint64_t id, std::uint64_t elapsed, Timestamp timestamp, EngineState engine,
                          bool ecu_connected, std::uint64_t errors, const Telemetry &telemetry, std::uint64_t now) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<double>::max_digits10)
        << prefix(id, 'S') << ',' << elapsed << ',';
    const bool clock_known = timestamp.unix_ms && timestamp.quality != ClockQuality::Unknown;
    if (clock_known) out << *timestamp.unix_ms;
    out << ',' << (clock_known ? (timestamp.quality == ClockQuality::Trusted ? "trusted" : "unverified") : "unknown")
        << ',' << name(engine) << ',' << (ecu_connected ? 1 : 0) << ',' << errors;
    for (const auto metric : {Metric::Oil, Metric::Intake, Metric::Voltage, Metric::Rpm}) {
        auto reading = telemetry.get(metric, now);
        if (!ecu_connected && (metric == Metric::Rpm || reading.source == Source::Ecu)) reading = {};
        out << ',';
        if (reading.value) out << *reading.value;
        out << ',' << name(reading.quality);
        if (metric == Metric::Voltage) out << ',' << name(reading.source);
    }
    return frame(out.str());
}

inline bool valid_metric(std::string_view value, std::string_view quality, double minimum) {
    if (quality == "valid" || quality == "simulated") {
        double number = 0;
        return parse(value, number) && std::isfinite(number) && number >= minimum;
    }
    return value.empty() && (quality == "missing" || quality == "unsupported" ||
                             quality == "error" || quality == "stale");
}

inline bool valid_sample(const std::array<std::string_view, 18> &fields) {
    std::int64_t timestamp = 0;
    std::uint64_t errors = 0;
    if (fields[5] == "unknown" ? !fields[4].empty()
        : ((fields[5] != "trusted" && fields[5] != "unverified") || !parse(fields[4], timestamp))) return false;
    if ((fields[6] != "unknown" && fields[6] != "running" && fields[6] != "stopped") ||
        (fields[7] != "0" && fields[7] != "1") || !parse(fields[8], errors)) return false;
    if (fields[15] != "none" && fields[15] != "mock" && fields[15] != "ecu" &&
        fields[15] != "adapter" && fields[15] != "battery") return false;
    return valid_metric(fields[9], fields[10], -273.15) &&
           valid_metric(fields[11], fields[12], -273.15) &&
           valid_metric(fields[13], fields[14], 0) && valid_metric(fields[16], fields[17], 0);
}
} // namespace record_format

enum class RecoveryState { Empty, Interrupted, Truncated, Corrupt, Completed, Unavailable };

struct Recovery {
    RecoveryState state = RecoveryState::Empty;
    std::uint64_t samples = 0;
    std::uint64_t valid_bytes = 0;
};

inline Recovery recover_recording(std::istream &input, std::uint64_t session_id) {
    Recovery result;
    std::uint64_t previous_elapsed = 0;
    while (input.peek() != std::char_traits<char>::eof()) {
        if (result.state == RecoveryState::Completed) {
            result.state = RecoveryState::Corrupt;
            return result;
        }
        std::string line;
        int next = input.get();
        while (next != '\n' && next != std::char_traits<char>::eof() && line.size() < record_format::max_line) {
            line.push_back(static_cast<char>(next));
            next = input.get();
        }
        if (next != '\n') {
            result.state = input.bad() ? RecoveryState::Unavailable
                : line.size() == record_format::max_line ? RecoveryState::Corrupt : RecoveryState::Truncated;
            return result;
        }

        const auto payload = record_format::checked_payload(line);
        std::array<std::string_view, 18> fields{};
        const auto count = payload ? record_format::split(*payload, fields) : 0;
        std::uint64_t id = 0;
        if (count < 3 || fields[0] != "1" || !record_format::parse(fields[1], id) || id != session_id) {
            result.state = RecoveryState::Corrupt;
            return result;
        }
        if (result.valid_bytes == 0) {
            if (count != 3 || fields[2] != "B") {
                result.state = RecoveryState::Corrupt;
                return result;
            }
            result.state = RecoveryState::Interrupted;
        } else {
            std::uint64_t elapsed = 0;
            if (count < 4 || !record_format::parse(fields[3], elapsed) || elapsed < previous_elapsed) {
                result.state = RecoveryState::Corrupt;
                return result;
            }
            if (count == 4 && fields[2] == "E") {
                result.state = RecoveryState::Completed;
            } else if (count == fields.size() && fields[2] == "S" && record_format::valid_sample(fields)) {
                ++result.samples;
            } else {
                result.state = RecoveryState::Corrupt;
                return result;
            }
            previous_elapsed = elapsed;
        }
        result.valid_bytes += line.size() + 1;
    }
    if (input.bad() || (!input.eof() && input.fail())) result.state = RecoveryState::Unavailable;
    return result;
}
} // namespace mfd
