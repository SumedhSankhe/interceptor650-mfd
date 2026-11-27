#include "mfd/telemetry.hpp"
#include <iostream>
#include <string>
#include <iomanip>
using namespace mfd;
int main(int argc, char** argv) {
    if (argc != 3) return 2;
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
    std::cout << '[';
    for (unsigned i = 0; i < 3; ++i) {
        auto r = model.get(static_cast<Metric>(i), now);
        if (i) std::cout << ',';
        std::cout << "{\"value\":";
        if (r.value) std::cout << std::fixed << std::setprecision(i == 2 ? 1 : 0) << (i != 2 && unit == "F" ? fahrenheit(*r.value) : *r.value);
        else std::cout << "null";
        std::cout << ",\"quality\":\"" << quality[static_cast<unsigned>(r.quality)] << "\",\"source\":\"" << (r.source == Source::Mock ? "MOCK" : "NONE") << "\"}";
    }
    std::cout << "]\n";
}
