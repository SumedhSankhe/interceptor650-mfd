# Proposed product scope

Status: working scope for Phase 0, subject to feasibility and design review. The project is a modular auxiliary motorcycle instrument for the 2023 US-spec Interceptor 650, not merely a telemetry display. Preserve the original instrument cluster and enclosure where practical.

## Functional requirements

| ID | Requirement | Priority |
|---|---|---|
| TEL-001 | Display engine oil temperature when a verified source is available | P0 |
| TEL-002 | Display intake air temperature when a verified source is available | P0 |
| TEL-003 | Display charging voltage, identifying the measurement source | P0 |
| NAV-001 | Display next turn maneuver | P1 |
| NAV-002 | Display distance to next maneuver | P1 |
| NAV-003 | Support iOS companion navigation over BLE | P1 |
| NAV-004 | Recover from phone disconnection; mark stale directions | P1 |
| UI-001 | Select telemetry or navigation mode | P1 |
| UI-002 | Hybrid view with navigation and selected telemetry | P1 |
| LOG-001 | Persist ride telemetry if storage is supported | P1 |
| LOG-002 | Optionally associate GPS position with telemetry | P2 |
| NAV-005 | Import predefined GPX routes | P2 |
| NAV-006 | Explore minimal route rendering | P2 |
| APP-001 | Post-ride analytics | P2 |
| NAV-007 | Explore phone-independent navigation | P3 |

P0: initial core; P1: major subsequent capability; P2: enhancement; P3: exploratory. Priorities are proposed, not delivery promises.

## Non-functional requirements

- NFR-001: Telemetry works without a phone; navigation is optional and must not block telemetry.
- NFR-002: Read-only diagnostic communication; no ECU configuration or actuator writes.
- NFR-003: Explicit stale/disconnected/unsupported state, never show mock data as live.
- NFR-004: Legible glanceable UI; no complex interaction required while riding. Test sunlight, lens reflections and gloves; select controls only after hardware verification.
- NFR-005: Validated ignition-switched power, transient protection, mechanical retention, weather resistance and acceptable parked current.
- NFR-006: Verify behavior under simultaneous BLE navigation and ECU transport activity; define latency and data-age budgets after measurement.
- NFR-007: Keep location logging opt-in and local by default; no credentials or private ride tracks in the public repository.

## Architecture direction

A phone computes routes and tracks progress; the ESP32 renders a compact normalized maneuver state. Standalone GNSS/route calculation is exploratory and may require extra hardware. The navigation provider, telemetry provider, application state, renderer and logger should have independent boundaries. Keep phone navigation independent of ECU connection. Candidate ECU transports include wired, BLE and Wi-Fi, none verified on the target motorcycle.

## Research gates

- Compare Apple MapKit and Google Navigation SDK for iOS: maneuver feed, rerouting, background location and locked-phone behavior, fees, licensing, external display terms and distribution requirements.
- Assess relevant projects as references, not adopted dependencies: [SteedPilot](https://github.com/deanthecoder/SteedPilot), [Motor_Display](https://github.com/annguyenkhanh/Motor_Display), [NavDisplay](https://github.com/nandushajahan/NavDisplay), [RoyalEnfieldObd](https://github.com/moca-owner-dev/RoyalEnfieldObd). Verify maturity, licenses, platform compatibility and actual implemented features before reuse.
- Prototype a versioned BLE GATT schema with maneuver, remaining distance, optional street name/ETA, timestamp/sequence, connection state and heartbeat. Evaluate pairing, reconnection, stale-state handling and radio coexistence.
- Investigate circular display desktop simulation and mock provider tests, including disconnect/reconnect and malformed data.
- Confirm exact board capabilities (including display, touch, storage and clock), and vehicle ECU compatibility. Do not assume access to Apple Maps or Google Maps app internals.

## Delivery sequence

Phase 0: requirements, architecture, ADRs, research and validation plan. Phase 1: protected original Tripper diagnostics, ESP32 bring-up and mock telemetry. Phase 2: verified live telemetry, mounting and protected power. Phase 3: phone-assisted navigation and hybrid display. Phase 4: logging/analytics and optional enhancements. Research can run in parallel; implementation remains gated on actual hardware and provider verification.

Out of initial scope: full offline routing, safety-critical instrument replacement, ECU tuning, and automatic control of motorcycle systems.

## Phase 0 design elaboration

See [requirements and acceptance criteria](requirements.md), [architecture](architecture.md), [navigation provider research](research/navigation.md), [BLE protocol proposal](protocol/navigation-ble.md), [decision records](adr/README.md) and [verification plan](verification.md). Provider selection is open: Google Navigation SDK commercial-use policy and Apple Maps external-display restrictions require clearance. Prioritize open-source routing feasibility and provider-free mock navigation before choosing an implementation. The original P0 telemetry priorities remain unchanged.
