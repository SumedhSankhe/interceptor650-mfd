# Verification and validation plan

The table defines the planned verification gates. Host evidence is recorded in the [V02 desktop browser record](verification-v02-browser.md) and the [Phase 1B V02/V05 record](verification-phase-1b.md). Preserve empirical results in [diagnostic log](diagnostics.md) or dated test records. A failed gate blocks dependent installation/road use. Host test success is not hardware qualification. Do not invent voltage, current or temperature thresholds; establish them from verified specifications and measurements.

| ID | Stage | Test and expected evidence | Requirements / issues |
|---|---|---|---|
| V01 | Bench | Original Tripper protected 12 V test; verified polarity, current limit, input voltage/current, 3.3 V rail, boot result. If boot succeeds inspect vehicle supply under load | NFR-005; #1 |
| V02 | Host/USB | Mock sample validity, unit conversion, stale transition, missing values, phone absent, viewport screenshots at 466 × 466; no clipped critical labels | TEL-001–003, UI-001/002, NFR-001/003; #3/#4 |
| V03 | USB | Exact board part/revision, vendor demo build/flash logs, toolchain versions, display/input/storage/RTC verification | #3 |
| V04 | Stationary bike | Confirm ECU/connector/protocol, allowlisted read-only responses, compare voltage to multimeter, validate temperature source/units and polling load | TEL-001–003, NFR-002; #2/#6 |
| V05 | Host | BLE frame exact vectors, malformed/fuzzed input, wrong version, invalid enum/flags, sequence replay, session reset, stale timeout and route state | NAV-001/002/004, NFR-003/007; #7 |
| V06 | Bench phone | Locked iPhone, background/permission change, Bluetooth disconnect/reconnect, app termination, radio coexistence with ECU-like traffic; log latency/drop/age | NAV-003/004, NFR-001/006; #7 |
| V07 | Controlled route replay | Synthetic and recorded permitted location fixtures: turn, roundabout, reroute, arrival, GPS loss; compare with expected maneuver; no obsolete arrow | NAV-001/002/003; #7 |
| V08 | Stationary installation | Direct sun, dark adaptation, lens glare, viewing angle, gloves, vibration-induced input; confirm stock instruments unobstructed | UI-001/002, NFR-004; #5/#6 |
| V09 | Storage | Missing/full card, repeated abrupt power-off, reboot, bad clock, queue overflow, opt-in GPS; completed sessions preserved, errors visible | LOG-001/002, NFR-007; #6 |
| V10 | Electrical/mechanical | Measure clearance, retention, current draw, regulated output over approved operating range, temperature and quiescent current; verify fuse/transient protection and sealing using safe methods | NFR-005; #5/#6 |
| V11 | Road readiness | Only after V03/V04/V08/V10 and applicable V06/V09 pass; safe controlled ride, compare baseline thermal and electrical measurements, no interactions while moving | #6 |

## Test record format

Date, test ID, revision/commit, device and firmware versions, test setup, instruments/calibration, inputs/conditions, expected behavior, raw observations, result (pass/fail/blocked), deviations and follow-up issue. Record quantitative distributions for radio age/latency, update rate, reconnect time, dropped samples, memory and power; initial timing goals in [requirements](requirements.md) are provisional. Keep sensitive identifiers and routes out of public records.

## Build and CI strategy

The host workflow compiles C++17 with warnings as errors and runs telemetry, navigation, Python renderer and Chromium browser tests. It records runner tool versions and uploads browser diagnostics on failure. Deterministic negative tests exercise the navigation parser, and desktop browser tests check circular clipping. Hardware testing remains a separate gate after vendor-example validation.

## Release gates

Phase 1: reproducible vendor demo + protected diagnostic result + mock UI. Phase 2: verified telemetry source and transport, safe stationary mechanical/electrical qualification. Phase 3: provider permissions, locked-phone reliability and coexistence; navigation failure cannot interrupt telemetry. Phase 4: verified storage/recovery and privacy handling. Do not market as safety-critical or road-ready based solely on prototype test results.
