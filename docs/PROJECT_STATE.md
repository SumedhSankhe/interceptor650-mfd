# Project state — hardware-validation checkpoint

Last updated: 2026-11-27 (Pacific time). This is a dated checkpoint, not a substitute for inspecting live repository state.

## Current milestone

Phase 1D is merged. Freeze major feature development until physical hardware arrives (expected approximately 2026-11-29; delivery not verified). Next action: Phase 2A bench validation. Desktop usability feedback and reproducible bug fixes are permitted; do not start speculative board integration.

Latest reviewed implementation baseline: [c074ed13](https://github.com/SumedhSankhe/interceptor650-mfd/commit/c074ed13fb4130ca373a37f800cbcaae9bf29f45) (squash merge of #13). Later documentation commits may follow.

| Milestone | Evidence |
|---|---|
| Phase 0: architecture and requirements | #8 merged |
| Phase 1A: portable telemetry, desktop renderer | #9 merged; browser verification #10 merged |
| Phase 1B: navigation decoder, simulator, CI | #11 merged |
| Phase 1C: engine/session lifecycle, recording, persistence | #12 merged |
| Phase 1D: integrated runtime and ride replay | #13 merged |

Phase 1D hosted PR CI was independently checked successful (run 45948186300); Codex reported strict Windows builds, four C++ suites, seven Python tests, 20 Chromium cases, MSVC AddressSanitizer and actionlint passing. Windows runs were not independently repeated by the reviewer. Nine captures are in simulator/screenshots/phase-1d; four were independently inspected. These are host tests, not hardware validation. See [verification](verification.md) and [Phase 1D evidence](verification-phase-1d.md).

## Product and decisions

Target: 2023 US-market Royal Enfield Interceptor 650, repurposing the nonfunctional OEM Tripper enclosure while retaining factory instruments. Initial priority: trustworthy oil temperature, intake air temperature and charging-voltage telemetry; navigation/hybrid modes and local recording supplement it. ECU label cross-reference identifies Bosch ME17.9.71 (Euro 4 family) with high confidence; diagnostic transport, pinout and PID availability remain unverified. No ECU writes or actuator commands. No fueling changes based solely on inferred temperatures.

- Portable C++17 domain and single-owner application runtime; value-owned snapshots, independent source lifecycles, bounded recording FIFO (four items provisional), newest sample dropped when full, ordered synchronization barrier retained. No embedded task implementation yet.
- Ignition-switched protected power, minimal additional hardware; no main battery, supercapacitor, external shutdown controller or custom shutdown circuitry. Required electrical adaptation and protection are exceptions. RTC backup optional/deferred.
- A recording session corresponds to a device power cycle. Fresh RPM > 0 means running; valid zero means stopped; missing/stale/error/unsupported means unknown. An observed running-to-stopped transition triggers a best-effort sync, not session termination. Stalls and kill-switch stops retain the same session; continue sampling available data. Key-off may remove power without warning.
- One-second sample and five-second sync targets, versioned CSV with per-record CRC-32, prefix recovery on cold boot, separate settings persistence. Never resurrect previous live data. Host file sync is not proof of microSD or directory metadata durability.
- Phone-independent telemetry and recording. Navigation uses the provider-independent 20-byte v1 frame; physical BLE and iOS companion/provider selection are open. No paid service adopted. See [navigation research](research/navigation.md).
- See [architecture](architecture.md), [runtime](runtime.md), [requirements](requirements.md), [ADRs](adr/README.md), [ride format](protocol/ride-recording.md) and [BLE contract](protocol/navigation-ble.md) for normative detail.

## Hardware status

See [BOM and enclosure notes](../hardware/README.md) and [original Tripper diagnostic readings](diagnostics.md). Ordered Waveshare ESP32-S3 1.43-inch AMOLED module, variable 3–24 V supply, mini grabbers, extended probes. Exact board revision, RTC, SD, power capabilities and fit remain unverified. Adjustable voltage supply is not established to be current-limited. Original Tripper passive tests did not establish a definitive failure. No powered test yet. No ECU adapter selected. Owner photographed the ECU label on 2026-11-27: RE2A050P1EU4003, RAC00312/a. External cross-references indicate Bosch ME17.9.71, HW 0261S18895, SW 10SW052973; this is not direct diagnostic readback. Evidence and remaining gates: [#2 ECU compatibility](https://github.com/SumedhSankhe/interceptor650-mfd/issues/2#issuecomment-7224263970).

## Next work: Phase 2A

1. Photograph delivered board, labels/connectors, PCB revision and packaging; identify exact vendor documentation and example versions.
2. Power Waveshare module by USB only. Record enumeration, vendor demo boot, display/touch, supported peripherals, RTC and storage observations. Verify exact hardware features; do not assume product-family specs apply.
3. Measure module and Tripper enclosure with calipers, test-fit without cutting; consider mounting, cable clearance, lens, sealing, heat and vibration.
4. Original Tripper bench test only with independently verified polarity and suitable current limiting, via original harness. Record input current/voltage and rail readings; stop if anomalous. No direct 3.3 V injection. If equipment is unsuitable, defer this test without delaying safe USB bring-up.
5. With exact hardware known, plan board-specific build and LVGL adaptation of existing runtime. Measure actual RAM/CPU/boot/current/latency; validate scheduling, bounded resource use and storage recovery.
6. Photograph motorcycle diagnostic connector (mating face and wire-entry side); verify pinout, electrical interface and protocol before selecting adapter. CAN is a candidate, not yet validated. Verify read-only supported PIDs, including candidate 010C (RPM), 015C (oil), 010F (IAT), 0142 (ECU voltage); distinguish adapter voltage from battery-terminal voltage.
7. Design and validate protected ignition-switched DC/DC supply, enclosure weather protection, physical power-cut recovery and road-test readiness. Never connect motorcycle 12 V directly to the Waveshare board.

Record exact procedures, raw measurements, photos where appropriate, results and unresolved gates in the relevant issue and verification record. Do not publish identifiers or personal information.

## Open issues (checked at checkpoint)

- #1 original Tripper protected bench test.
- #2 ECU and diagnostic connector compatibility.
- #3 Waveshare board bring-up.
- #4 mock dashboard (host implemented; remaining verification must be assessed).
- #5 automotive power and enclosure.
- #6 live telemetry and road readiness.
- #7 iOS navigation feasibility.

Open physical gates include USB/display, glare/readability, glove operation, board scheduling, actual ECU data, BLE pairing and companion, RTC quality, power interruption, microSD integrity/endurance and environmental testing. Live SD replacement is unsupported pending mount identity design. Session IDs are scoped to the retained directory. Archive scan and queue sizing need measured embedded evidence.

## Resume

Read [HANDOFF.md](HANDOFF.md) and [AGENTS.md](../AGENTS.md), reconcile this dated record with live main, open PRs and issues, then proceed only with the next authorized task. Update this checkpoint whenever a milestone or blocker changes.
