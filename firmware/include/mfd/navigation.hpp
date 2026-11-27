#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>

namespace mfd {
enum class NavState : std::uint8_t { Idle, Active, Rerouting, Arrived, Unavailable, Disconnected, Stale };
enum class Maneuver : std::uint8_t { Unknown, Straight, Left, Right, UTurn, Roundabout, SlightLeft, SlightRight, KeepLeft, KeepRight };

struct NavigationSnapshot {
    NavState state = NavState::Unavailable;
    Maneuver maneuver = Maneuver::Unknown;
    std::optional<std::uint32_t> distance_m;
    std::optional<std::uint8_t> exit_number;
    std::uint32_t session_id = 0;
    std::uint32_t sequence = 0;
    std::uint64_t received_ms = 0;
};

inline constexpr std::size_t navigation_frame_size = 20;

inline std::optional<NavigationSnapshot> decode_navigation(const std::uint8_t *bytes, std::size_t size) {
    if (!bytes || size != navigation_frame_size || bytes[0] != 1 ||
        bytes[1] > static_cast<unsigned>(NavState::Unavailable) ||
        bytes[2] > static_cast<unsigned>(Maneuver::KeepRight) ||
        (bytes[3] & 0xfc) || bytes[17] || bytes[18] || bytes[19]) return std::nullopt;

    auto le32 = [bytes](std::size_t offset) {
        return std::uint32_t{bytes[offset]} | (std::uint32_t{bytes[offset + 1]} << 8) |
               (std::uint32_t{bytes[offset + 2]} << 16) | (std::uint32_t{bytes[offset + 3]} << 24);
    };
    NavigationSnapshot snapshot;
    snapshot.state = static_cast<NavState>(bytes[1]);
    snapshot.maneuver = static_cast<Maneuver>(bytes[2]);
    snapshot.session_id = le32(4);
    snapshot.sequence = le32(8);
    const auto distance = le32(12);
    const auto exit_number = bytes[16];
    if (!snapshot.sequence) return std::nullopt;

    if (snapshot.state != NavState::Active) {
        if (snapshot.maneuver != Maneuver::Unknown || bytes[3] || distance || exit_number) return std::nullopt;
        return snapshot;
    }

    if (snapshot.maneuver == Maneuver::Unknown || !(bytes[3] & 1)) return std::nullopt;
    snapshot.distance_m = distance;
    if (bytes[3] & 2) {
        if (snapshot.maneuver != Maneuver::Roundabout || !exit_number) return std::nullopt;
        snapshot.exit_number = exit_number;
    } else if (exit_number) {
        return std::nullopt;
    }
    return snapshot;
}

class Navigation {
public:
    static constexpr std::uint64_t stale_ms = 3000;

    // Call only for a new authorized link. Repeated notifications cannot reset ordering.
    void connect() { connected_ = true; }

    void disconnect() {
        if (accepted_) previous_session_ = accepted_->session_id;
        accepted_.reset();
        connected_ = false;
    }

    bool ingest(const std::uint8_t *bytes, std::size_t size, std::uint64_t now) {
        if (!connected_) return false;
        auto next = decode_navigation(bytes, size);
        if (!next) return false;
        if (accepted_) {
            if (next->session_id != accepted_->session_id || next->sequence <= accepted_->sequence ||
                now < accepted_->received_ms) return false;
        } else if (next->sequence != 1 || previous_session_ == next->session_id) {
            return false;
        }
        next->received_ms = now;
        accepted_ = next;
        return true;
    }

    NavigationSnapshot get(std::uint64_t now) const {
        NavigationSnapshot snapshot = accepted_.value_or(NavigationSnapshot{});
        if (!connected_) snapshot.state = NavState::Disconnected;
        if (snapshot.state == NavState::Active &&
            (now < snapshot.received_ms || now - snapshot.received_ms >= stale_ms)) {
            snapshot.state = NavState::Stale;
            snapshot.maneuver = Maneuver::Unknown;
            snapshot.distance_m.reset();
            snapshot.exit_number.reset();
        }
        return snapshot;
    }

private:
    bool connected_ = false;
    std::optional<std::uint32_t> previous_session_;
    std::optional<NavigationSnapshot> accepted_;
};
} // namespace mfd
