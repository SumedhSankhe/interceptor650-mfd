#include "runtime_fixture.hpp"

int main(int argc, char **argv) {
    if (argc != 14) return 2;
    std::array<std::uint32_t, 10> input{};
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (!simulator::parse_number(argv[4 + i], input[i])) return 2;
    }
    if (input[0] > 2 || input[1] > 1 || input[2] > 1) return 2;
    const mfd::Settings preferences{static_cast<mfd::TemperatureUnit>(input[1]),
        static_cast<mfd::DistanceUnit>(input[2]), static_cast<mfd::DisplayMode>(input[0])};
    simulator::RuntimeFixture fixture(preferences);
    const std::string_view ride = argv[2];
    const std::string_view replay = argv[3];
    if (replay == "timeline") {
        if (!fixture.timeline(input[9])) return 2;
    } else if (replay == "fixtures") {
        if (!fixture.replay(ride) || !fixture.apply_fixtures(argv[1],
            {input[3], input[4], input[5], input[6], input[7], input[8]}, ride == "phone-independent")) return 2;
    } else {
        return 2;
    }
    simulator::write_snapshot(fixture.snapshot());
}
