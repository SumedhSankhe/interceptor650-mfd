#include "mfd/navigation.hpp"
#include "mfd/telemetry.hpp"
#include <array>
#include <cassert>
#include <iostream>
#include <limits>
#include <random>

using namespace mfd;

namespace {
using Frame = std::array<std::uint8_t, navigation_frame_size>;

constexpr Frame right = {1, 1, 3, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0x90, 1, 0, 0, 0, 0, 0, 0};
constexpr Frame rerouting = {1, 2, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

void put(Frame &frame, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) frame[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
}

bool ingest(Navigation &nav, const Frame &frame, std::uint64_t now) {
    return nav.ingest(frame.data(), frame.size(), now);
}

void check_states_and_maneuvers() {
    const auto decoded = decode_navigation(right.data(), right.size());
    assert(decoded && decoded->state == NavState::Active && decoded->maneuver == Maneuver::Right);
    assert(decoded->distance_m == 400u && !decoded->exit_number && decoded->session_id == 1 && decoded->sequence == 1);

    Navigation nav;
    nav.connect();
    assert(nav.get(0).state == NavState::Unavailable);
    assert(ingest(nav, right, 0));
    assert(ingest(nav, rerouting, 1));
    assert(nav.get(1).state == NavState::Rerouting && !nav.get(1).distance_m);

    std::uint32_t sequence = 2;
    for (auto state : {NavState::Idle, NavState::Arrived, NavState::Unavailable}) {
        auto frame = rerouting;
        frame[1] = static_cast<std::uint8_t>(state);
        put(frame, 8, ++sequence);
        assert(ingest(nav, frame, sequence));
        const auto snapshot = nav.get(sequence + Navigation::stale_ms);
        assert(snapshot.state == state && snapshot.maneuver == Maneuver::Unknown);
        assert(!snapshot.distance_m && !snapshot.exit_number);
    }

    for (unsigned kind = 1; kind <= 9; ++kind) {
        auto frame = right;
        frame[2] = static_cast<std::uint8_t>(kind);
        put(frame, 8, ++sequence);
        put(frame, 12, 0);
        if (kind == static_cast<unsigned>(Maneuver::Roundabout)) {
            frame[3] = 3;
            frame[16] = 255;
        }
        assert(ingest(nav, frame, sequence));
        const auto snapshot = nav.get(sequence);
        assert(snapshot.state == NavState::Active && snapshot.maneuver == static_cast<Maneuver>(kind));
        assert(snapshot.distance_m == 0u);
        assert(snapshot.exit_number.has_value() == (kind == static_cast<unsigned>(Maneuver::Roundabout)));
        if (snapshot.exit_number) assert(*snapshot.exit_number == std::uint8_t{255});
    }

    auto frame = right;
    frame[2] = static_cast<std::uint8_t>(Maneuver::Roundabout);
    put(frame, 4, 0x01020304);
    put(frame, 8, 0xa0b1c2d3);
    put(frame, 12, 0x8090a0b0);
    const auto le = decode_navigation(frame.data(), frame.size());
    assert(le && le->session_id == 0x01020304 && le->sequence == 0xa0b1c2d3);
    assert(le->distance_m == 0x8090a0b0 && !le->exit_number);
}

void check_invalid_packets() {
    Navigation nav;
    nav.connect();
    assert(ingest(nav, right, 0));
    assert(!nav.ingest(nullptr, right.size(), 1000));
    for (std::size_t size = 0; size < right.size(); ++size) {
        assert(!nav.ingest(right.data(), size, 1000));
    }
    const std::array<std::uint8_t, 21> extended{};
    assert(!nav.ingest(extended.data(), extended.size(), 1000));

    auto candidate = right;
    put(candidate, 8, 2);
    auto invalid = [&](std::size_t offset, std::uint8_t value) {
        auto frame = candidate;
        frame[offset] = value;
        assert(!ingest(nav, frame, 2999));
    };
    invalid(0, 2);
    invalid(1, 5);
    invalid(1, 6);
    invalid(1, 255);
    invalid(2, 0);
    invalid(2, 10);
    invalid(3, 0);
    invalid(3, 3); // Exit valid for an ordinary turn.
    invalid(3, 5);
    invalid(8, 0);
    invalid(16, 1); // Exit field without a flag.
    for (std::size_t offset = 17; offset < right.size(); ++offset) invalid(offset, 1);

    auto roundabout = candidate;
    roundabout[2] = static_cast<std::uint8_t>(Maneuver::Roundabout);
    roundabout[3] = 3;
    assert(!ingest(nav, roundabout, 2999)); // Flag set with a zero exit.

    candidate = rerouting;
    invalid(2, 3);
    invalid(3, 1);
    invalid(12, 1);
    invalid(16, 1);

    std::mt19937 random(650);
    for (unsigned i = 0; i < 1024; ++i) {
        Frame frame{};
        for (auto &byte : frame) byte = static_cast<std::uint8_t>(random());
        assert(!ingest(nav, frame, 2999)); // Wrong envelope or unrelated session.
        frame = rerouting;
        frame[17 + random() % 3] = static_cast<std::uint8_t>(1 + random() % 255);
        assert(!ingest(nav, frame, 2999));
    }

    assert(nav.get(2999).received_ms == 0 && nav.get(2999).state == NavState::Active);
    const auto stale = nav.get(3000);
    assert(stale.state == NavState::Stale && stale.sequence == 1);
    assert(stale.maneuver == Maneuver::Unknown && !stale.distance_m && !stale.exit_number);
    assert(ingest(nav, rerouting, 3000));
    assert(nav.get(3000).state == NavState::Rerouting);
}

void check_sessions_and_independence() {
    Navigation nav;
    Telemetry telemetry;
    assert(nav.get(0).state == NavState::Disconnected && !ingest(nav, right, 0));
    assert(telemetry.ingest(Metric::Oil, {108, Source::Mock, Quality::Simulated, 1000}, 1000));
    nav.connect();
    assert(!ingest(nav, rerouting, 1000)); // Initial sequence must be one.
    assert(ingest(nav, right, 1000));
    assert(nav.get(999).state == NavState::Stale);
    nav.connect(); // Repeated link notification cannot reopen the ordering window.
    assert(!ingest(nav, right, 2000));
    assert(!ingest(nav, rerouting, 999));
    auto next = right;
    put(next, 4, 2);
    assert(!ingest(nav, next, 2000));
    put(next, 4, 1);
    put(next, 8, 3); // Full snapshots allow sequence gaps.
    assert(ingest(nav, next, 2000));
    assert(!ingest(nav, rerouting, 2001));
    auto wrap = next;
    put(wrap, 8, std::numeric_limits<std::uint32_t>::max());
    assert(ingest(nav, wrap, 2002));
    assert(!ingest(nav, right, 2003));
    nav.disconnect();
    assert(nav.get(2003).state == NavState::Disconnected && !nav.get(2003).distance_m);
    assert(telemetry.get(Metric::Oil, 2003).value == 108);
    nav.connect();
    assert(nav.get(2003).state == NavState::Unavailable && !ingest(nav, right, 2003));
    put(next, 4, 2);
    put(next, 8, 1);
    assert(ingest(nav, next, 2004));
    telemetry.disconnect();
    assert(nav.get(5003).state == NavState::Active);
    assert(nav.get(5004).state == NavState::Stale);
    put(next, 8, 2);
    assert(ingest(nav, next, 5004) && nav.get(5004).state == NavState::Active);
    assert(!telemetry.get(Metric::Oil, 5004).value);

    nav.disconnect();
    nav.connect();
    put(next, 4, 0); // The current v1 spec assigns no session sentinel.
    put(next, 8, 1);
    assert(ingest(nav, next, 6000));
}
} // namespace

int main() {
    check_states_and_maneuvers();
    check_invalid_packets();
    check_sessions_and_independence();
    std::cout << "navigation assertions passed\n";
}
