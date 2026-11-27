#include "mfd/telemetry.hpp"
#include "navigation_fixture.hpp"
#include "ride_fixture.hpp"
#include <iostream>
#include <string>
#include <iomanip>
using namespace mfd;
int main(int argc, char** argv) {
    if (argc == 3 && std::string_view(argv[1]) == "--ride") return simulator::ride_fixture(argv[2]) ? 0 : 2;
    if (argc != 9) return 2;
    std::array<std::uint32_t, 6> inputs{};
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        if (!simulator::parse_number(argv[i + 3], inputs[i])) return 2;
    }
    NavigationSnapshot navigation;
    if (!simulator::navigation_fixture({inputs[0], inputs[1], inputs[2], inputs[3], inputs[4], inputs[5]},
                                       navigation)) return 2;
    const std::string scenario = argv[1], unit = argv[2];
    if ((scenario != "normal" && scenario != "stale" && scenario != "unsupported" && scenario != "disconnected") || (unit != "C" && unit != "F")) return 2;
    Telemetry model;
    const std::uint64_t now = scenario == "stale" ? 4000 : 1000;
    if (scenario != "disconnected") {
        model.ingest(Metric::Oil, {108, Source::Mock, Quality::Simulated, 1000}, now);
        model.ingest(Metric::Intake, {32, Source::Mock, Quality::Simulated, 1000}, now);
        model.ingest(Metric::Voltage, {14.2, Source::Mock, Quality::Simulated, 1000}, now);
        if (scenario == "unsupported") model.ingest(Metric::Oil, {std::nullopt, Source::Mock, Quality::Unsupported, 1000}, now);
    }
    const char* quality[] = {"MISSING", "VALID", "SIMULATED", "UNSUPPORTED", "ERROR", "STALE"};
    std::cout << "{\"telemetry\":[";
    for (unsigned i = 0; i < 3; ++i) {
        auto r = model.get(static_cast<Metric>(i), now);
        if (i) std::cout << ',';
        std::cout << "{\"value\":";
        if (r.value) std::cout << std::fixed << std::setprecision(i == 2 ? 1 : 0) << (i != 2 && unit == "F" ? fahrenheit(*r.value) : *r.value);
        else std::cout << "null";
        std::cout << ",\"quality\":\"" << quality[static_cast<unsigned>(r.quality)] << "\",\"source\":\"" << (r.source == Source::Mock ? "MOCK" : "NONE") << "\"}";
    }
    std::cout << "],\"navigation\":";
    simulator::write_navigation(navigation);
    std::cout << "}\n";
}
