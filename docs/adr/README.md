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
