#pragma once
#include "mfd/navigation.hpp"
#include <array>
#include <charconv>
#include <iostream>
#include <string_view>

namespace simulator {
struct NavigationFixture {
    unsigned state;
    unsigned maneuver;
    std::uint32_t distance;
    unsigned exit_number;
    std::uint32_t age;
    unsigned connection;
};

inline bool parse_number(std::string_view text, std::uint32_t &value) {
    const auto end = text.data() + text.size();
    const auto result = std::from_chars(text.data(), end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

inline bool navigation_fixture(const NavigationFixture &fixture, mfd::NavigationSnapshot &snapshot) {
    using mfd::NavState;
    using mfd::Maneuver;
    if (fixture.state > static_cast<unsigned>(NavState::Stale) ||
        fixture.maneuver < static_cast<unsigned>(Maneuver::Straight) ||
        fixture.maneuver > static_cast<unsigned>(Maneuver::KeepRight) ||
        fixture.exit_number > 255 || fixture.connection > 2) return false;

    std::array<std::uint8_t, mfd::navigation_frame_size> frame{};
    frame[0] = 1;
    const auto put = [&frame](std::size_t offset, std::uint32_t value) {
        for (unsigned i = 0; i < 4; ++i) {
            frame[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
        }
    };
    const auto active = [&] {
        frame[1] = static_cast<std::uint8_t>(NavState::Active);
        frame[2] = static_cast<std::uint8_t>(fixture.maneuver);
        frame[3] = 1;
        put(12, fixture.distance);
        if (fixture.maneuver == static_cast<unsigned>(Maneuver::Roundabout) && fixture.exit_number) {
            frame[3] |= 2;
            frame[16] = static_cast<std::uint8_t>(fixture.exit_number);
        }
    };
    mfd::Navigation navigation;
    navigation.connect();
    put(4, 1);
    put(8, 1);
    active();
    if (!navigation.ingest(frame.data(), frame.size(), 0)) return false;
    if (fixture.connection || fixture.state == static_cast<unsigned>(NavState::Disconnected)) {
        navigation.disconnect();
        if (fixture.state == static_cast<unsigned>(NavState::Disconnected)) {
            snapshot = navigation.get(1);
            return true;
        }
        navigation.connect();
    }
    if (fixture.connection != 1) {
        put(4, fixture.connection == 2 ? 2 : 1);
        put(8, fixture.connection == 2 ? 1 : 2);
        if (fixture.state < static_cast<unsigned>(NavState::Unavailable) + 1 &&
            fixture.state != static_cast<unsigned>(NavState::Active)) {
            frame[1] = static_cast<std::uint8_t>(fixture.state);
            frame[2] = frame[3] = frame[16] = 0;
            put(12, 0);
        }
        if (!navigation.ingest(frame.data(), frame.size(), 1)) return false;
    }
    const auto age = fixture.state == static_cast<unsigned>(NavState::Stale)
        ? mfd::Navigation::stale_ms : fixture.age;
    snapshot = navigation.get(1 + age);
    return true;
}

inline void write_navigation(const mfd::NavigationSnapshot &snapshot) {
    std::cout << "{\"state\":" << static_cast<unsigned>(snapshot.state)
              << ",\"maneuver\":" << static_cast<unsigned>(snapshot.maneuver) << ",\"distance_m\":";
    if (snapshot.distance_m) std::cout << *snapshot.distance_m;
    else std::cout << "null";
    std::cout << ",\"exit_number\":";
    if (snapshot.exit_number) std::cout << static_cast<unsigned>(*snapshot.exit_number);
    else std::cout << "null";
    std::cout << ",\"session\":" << snapshot.session_id << ",\"sequence\":" << snapshot.sequence << '}';
}
} // namespace simulator
