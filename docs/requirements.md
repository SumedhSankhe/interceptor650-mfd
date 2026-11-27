# Requirements and acceptance criteria

Status: proposed Phase 0 baseline, 2026-11-26. Stable IDs and priorities originate in [scope](scope.md). Tests reference [verification](verification.md). P0 core, P1 next major capability, P2 enhancement, P3 exploratory. Unverified sources must not be replaced by fabricated values. Dependencies are gates, not promises.

| ID | P | Acceptance criterion | Verification / gate |
|---|---|---|---|
| TEL-001 | P0 | Show oil temperature in °C or configured °F from verified source; unsupported/stale explicit | V02, V04; issue #2 |
| TEL-002 | P0 | Show intake air temperature with units and freshness; unsupported/stale explicit | V02, V04; #2 |
| TEL-003 | P0 | Label voltage measurement source (ECU, adapter, or calibrated direct battery measurement); validate against reference | V02, V04; #2 |
| TEL-004 | P1 | Use verified RPM for recording and engine state. Distinguish valid zero from unavailable, error or stale readings. | V02, V04; #6 |
| ENG-001 | P1 | Derive running, stopped or unknown from fresh RPM. A direct running-to-stopped observation requests best-effort synchronization and leaves the ride session open. | V02, V09; #6 |
| NAV-001 | P1 | Show normalized maneuver, including unknown/unavailable state; rerouting removes obsolete turn | V05, V07; provider approval |
| NAV-002 | P1 | Display nonnegative next-turn distance with consistent unit conversion; no false precision | V05, V07 |
| NAV-003 | P1 | Paired iOS companion sends documented BLE messages while active and locked | V06, V07; #7 |
| NAV-004 | P1 | Disconnect clears active directions; reconnect requires fresh snapshot | V05, V06 |
| UI-001 | P1 | Select telemetry/navigation with safe interaction; always retain access to telemetry without phone | V02, V08 |
| UI-002 | P1 | Hybrid screen displays maneuver plus essential available telemetry, with no clipped critical information | V02, V08 |
| LOG-001 | P1 | Record versioned, source-tagged telemetry, individual qualities, and clock quality. Recover the valid prefix of an interrupted log and preserve previously completed logs | V09; verified storage/clock |
| LOG-003 | P1 | Open one ride session per ignition-powered boot. Continue through stalls, kill-switch stops, ECU gaps and phone disconnects. Infer interrupted sessions on subsequent boot and allocate a fresh ID. | V02, V09; #6 |
| LOG-004 | P1 | Persist changed display settings with a version. Use safe defaults for absent, invalid or unsupported data. | V02, V09; #6 |
| LOG-002 | P2 | Position associated only with opt-in, valid GPS; provider-owned content excluded | V09; privacy and provider gate |
| NAV-005 | P2 | Import valid GPX and reject malformed inputs; specify off-route behavior | Future test design |
| NAV-006 | P2 | Evaluate simplified route-map rendering within readability and provider permissions | Future spike |
| APP-001 | P2 | Analyze local exported sessions; preserve units and missing data | Future test design |
| NAV-007 | P3 | Establish feasibility of phone-independent guidance and extra hardware | Research only |

## Non-functional

| ID | Acceptance criterion | Verification |
|---|---|---|
| NFR-001 | Phone absence, disconnect and navigation failure do not halt ECU acquisition or telemetry rendering | V02, V06 |
| NFR-002 | Diagnostic allowlist limited to reviewed read-only requests; no arbitrary ECU command forwarding | V04; code review |
| NFR-003 | Per-source quality and monotonic age drive rendering; simulated data clearly marked; invalid input never appears current | V02, V05 |
| NFR-004 | Glanceable typography, no typing/menu traversal needed in motion, daylight and gloves evaluated; stock gauges retained | V08 |
| NFR-005 | Prefer functions already integrated into the verified Waveshare module. Use verified ignition-switched supply, necessary electrical compatibility and automotive protection. Qualify transient/reverse protection, fuse, retention, sealing and thermal margin before road use. Use software recovery for abrupt power loss and defer optional RTC backup | V01, V10 |
| NFR-006 | Measure BLE/Wi-Fi coexistence under load, frame age, drops and latency; no silent stale directions | V06 |
| NFR-007 | Pairing authorization; local opt-in location logging; no secrets or identifiable tracks in repository | V05, V09; review |

The ignition architecture excludes a main backup battery, supercapacitor, external
shutdown controller and custom shutdown circuitry. The design assumes abrupt
loss at any time and does not depend on receiving zero RPM before cutoff.
Telemetry and recording remain independent of phone connectivity.

## Proposed engineering budgets

For fixture and bench tests only, not measured performance: target telemetry updates 1–2 Hz if ECU allows; stale after 3 s without a valid sample; navigation full snapshot ~1 Hz while active, stale after 3 s without accepted valid state; renderer target 10 Hz or faster as needed with event-driven updates. Use monotonic time, not wall clock, for freshness. Measure jitter, CPU/memory, radio latency and ECU load, then revise budgets before road testing. Do not define engine temperature or charging warning thresholds until supported by service data and calibrated observations.

## Dependencies and non-goals

Original Tripper root cause, actual board revision, ECU identity/protocol, connector polarity under load, regulator design, installation fit, provider terms and locked-phone behavior remain unresolved. No full offline maps, tuning or control of motorcycle systems in the initial scope. No compulsory cloud service for telemetry. Navigation unavailable is an acceptable degraded mode; incorrect directions presented as current are not.
