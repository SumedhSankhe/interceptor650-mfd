# Architecture decision records

Status: proposed Phase 0 decisions, pending owner review. Supersede with new ADRs rather than silently rewriting accepted history. Each record states context, alternatives, decision, consequences and evidence needed.

## ADR-001 — Modular, phone-optional instrument

**Status:** proposed. **Context:** telemetry originally motivated the project; navigation is an extension. **Alternatives:** single monolithic loop; phone as mandatory processing hub; independent providers on ESP32. **Decision:** choose independent telemetry and navigation domains with common state reducer, single-owner renderer and optional logger. **Consequences:** more explicit contracts and test fixtures; degraded operation remains possible. **Gate:** host and bench tests prove phone failure cannot block telemetry.

## ADR-002 — Navigation provider selection deferred

**Status:** proposed, provider unresolved. **Context:** iOS external display needed; Google SDK policy commercial-use only; MapKit external-display permissions uncertain. **Alternatives:** Google SDK, MapKit, MapLibre/Valhalla, GPX-only. **Decision:** mock provider and BLE protocol first; prioritize open-source routing feasibility spike, no provider adoption until policy, cost and real-device behavior reviewed. **Consequences:** no live navigation in initial phase; avoid vendor lock-in. **Gate:** [research](../research/navigation.md), issue #7, locked-phone test and provider/license review.

## ADR-003 — Read-only ECU transport and evidence-first selection

**Status:** proposed. **Context:** exact 2023 US ECU and connector unknown. **Alternatives:** verified ELM-compatible Wi-Fi/BLE; wired interface; independent sensor where necessary. **Decision:** choose only after identification and read-only response evidence; avoid Bluetooth Classic on ESP32-S3. **Consequences:** delays live telemetry but avoids unsupported purchases. **Gate:** issue #2 and voltage-source validation.

## ADR-004 — Host simulation before hardware integration

**Status:** proposed. **Context:** board revision and framework not yet verified. **Alternatives:** hardware-only sketch vs portable state model + host tests + board HAL. **Decision:** host-first state/decoder tests and circular LVGL simulation, then vendor-example bring-up; pin actual framework after verification. **Consequences:** faster iteration but simulator cannot prove brightness, touch, timing or electrical behavior. **Gate:** issue #3 and #4.

## ADR-005 — Preserve original enclosure and protect power

**Status:** proposed. **Context:** salvageable OEM electronics and estimated fit; vehicle transients and sealing unknown. **Alternatives:** cutting housing now; external enclosure; non-destructive test fit. **Decision:** diagnose original first, verify actual dimensions, design protected ignition-switched power, retain original cluster. **Consequences:** mechanical integration delayed until measurements. **Gate:** issues #1, #5 and #6.

## ADR-006 — Local and privacy-conscious logging

**Status:** proposed. **Context:** useful thermal trend data; storage/clock unverified and navigation content may be restricted. **Alternatives:** cloud-first, local file, no logging. **Decision:** optional local telemetry files with source/time-quality metadata; GPS opt-in, no provider-owned map content persisted. **Consequences:** export and power-loss recovery required; storage failure must not stop display. **Gate:** V09 and licensing review.

## ADR-007 — Exercise navigation packets in the host simulator

**Status:** proposed for Phase 1B review. **Context:** Static SVG fixtures left v1 decoding and stale guidance unverified. **Alternatives:** render synthetic state directly or build a hardware transport first. **Decision:** replay deterministic v1 snapshots, link events and a supplied monotonic clock through the portable decoder and model. Keep telemetry independent and use GitHub Actions to exercise the host build and browser. **Consequences:** HTTP requests can reproduce any fixture without shared server state. The runtime remains local with only the Python standard library and C++ fixture. **Gate:** V02/V05 host review, then V06/V07 transport and provider validation. See the [v1 host interpretation](../protocol/navigation-ble.md#host-interpretation-for-protocol-review).

## ADR-008 — Ignition-switched power and software recovery

**Status:** owner-approved Phase 1C design. **Context:** The motorcycle supply
can disappear without a final ECU update. **Decision:** Prefer capabilities
integrated into the verified Waveshare module. Use an ignition-switched supply
and limit external circuitry to compatibility and automotive protection.
Exclude a main battery, supercapacitor, external shutdown controller and
custom shutdown circuitry. Defer optional RTC backup. **Consequences:** Assume
abrupt power loss at any time. Use host-tested recovery and qualify the
actual power/storage behavior independently. **Gate:** NFR-005, V09 and V10,
Issue #6.

## ADR-009 — One recoverable ride session per ignition-powered boot

**Status:** Phase 1C host implementation for review. **Context:** Zero RPM can
mean several things, and an ECU failure provides no stop observation.
**Decision:** Use fresh RPM for engine state and use the power lifecycle for
session boundaries. An observed running-to-stopped transition requests
best-effort sync. Use versioned, checksummed CSV records with a readable
prefix and separate, replace-on-change settings. **Consequences:** The same
session continues through engine stops, ECU outages and phone disconnects.
Missing final records are inferred after reboot, and wall clock is optional.
**Gate:** ENG-001, LOG-001/003/004, V02 and V09. See the
[recording contract](../protocol/ride-recording.md).

## ADR-010 — Single-owner application state with bounded storage work

**Status:** Phase 1D host implementation for review. **Context:** Independent
fixtures and mutable model reads could disagree when integration introduced
worker delays. **Decision:** Serialize source events in a portable reducer,
publish latest-value snapshots, and transfer owned samples through a
fixed-capacity FIFO. Preserve stop sync through a tail barrier even when
the sample is dropped. **Consequences:** The owner has no storage I/O.
Workers preserve the schedule's capture order, and the host exercises
delay without adding threads. Archives and volume identity remain
storage-adapter concerns. **Gate:** RT-001/002, V02/V05/V09, Issues #4,
#6 and #7. See the [runtime contract](../runtime.md).
