# Phase 1B host verification

| Field | Record |
|---|---|
| Date | 2026-11-26 (Pacific time) |
| Scope | V02 and V05 desktop host coverage for [Issue #7](https://github.com/SumedhSankhe/interceptor650-mfd/issues/7) |
| Revision | `feature/navigation-foundation`, based on `7ec21ba` |
| Host | Windows x64 build 26200, Visual Studio 2022 17.11.2, MSVC 14.41.34120 |
| Tools | Python 3.13.5, Node.js 22.20.0, npm 10.9.3, Playwright 1.65.1, headless Chromium 159.0.8154.6 (revision 1255) |
| Inputs | Published v1 byte vectors, synthetic frames and explicit monotonic times |
| Result | V02 and V05 host tests passed. V02 physical display and USB evidence remain open. |

## Results

| Coverage | Evidence |
|---|---|
| V05 · NAV-001, NAV-002, NAV-004, NFR-003 | Strict C++17 navigation assertions passed. Both published vectors decode. Tests cover all wire maneuvers and states, endian order, zero distance, roundabout flags, truncation, extended frames, reserved bits, unsupported values, replay, sequence exhaustion, session reset and recovery. Fixed seed 650 produces 1024 arbitrary frames and 1024 invalid reserved-byte variations. |
| V05 · freshness and independent lifecycles | The 2999 ms snapshot still shows guidance. At 3000 ms it suppresses the arrow, distance and exit. Invalid frames preserve the original reception time. Simulation and assertions confirm that disconnecting either source leaves the other source available. |
| V02 · TEL-001–003, UI-001/002, NFR-001/003 | Strict C++17 telemetry assertions passed. All four renderer tests passed, including 168 selector combinations, invalid inputs, navigation state output and the three prior SVG snapshots. |
| V02 · navigation display | All 16 Chromium cases passed, covering nine distinct maneuver graphics, roundabout exits, 2999/3000 ms timeout, arrival, reroute, reconnect, independent units and degradation. No console errors, JavaScript exceptions or failed requests occurred on the exercised interaction paths. |
| V02 · geometry and process cleanup | Chromium checked the 466 × 466 viewport, transparent corners and conservative text and path bounds including transforms and strokes. Controls fit below the square display at 280 px browser width. The new direct-child Python launcher completed and exited cleanly on Windows. |

The protocol's [host interpretation](protocol/navigation-ble.md#host-interpretation-for-protocol-review)
records the handling of zero session IDs, active/unknown ambiguity, reconnect
and sequence exhaustion. The wire layout and test vectors are unchanged.
The simulator replays a deterministic session per request so all test
conditions are reproducible without shared server state.

## Browser evidence

The PNGs are full-page Chromium captures from a 1100 × 680 viewport at
device scale 1.

- Maneuvers — [straight](../simulator/screenshots/phase-1b/maneuver-straight.png),
  [left turn](../simulator/screenshots/phase-1b/maneuver-left.png),
  [slight left](../simulator/screenshots/phase-1b/maneuver-slight-left.png),
  [keep right](../simulator/screenshots/phase-1b/maneuver-keep-right.png),
  [U-turn](../simulator/screenshots/phase-1b/maneuver-uturn.png), and
  [roundabout](../simulator/screenshots/phase-1b/maneuver-roundabout.png).
- States — [normal hybrid](../simulator/screenshots/phase-1b/hybrid.png),
  [rerouting](../simulator/screenshots/phase-1b/navigation-rerouting.png),
  [expired guidance](../simulator/screenshots/phase-1b/elapsed-stale.png),
  [arrived](../simulator/screenshots/phase-1b/arrived.png), and
  [combined degradation](../simulator/screenshots/phase-1b/combined-degraded.png).

The roundabout number sits inside the loop with a separate exit arrowhead.
Distance text is centered beneath the symbol, and the two hybrid telemetry
columns align below guidance. White foreground and muted supporting labels
remain distinct on the dark face at desktop size. Secondary text scales
down in the narrow view. Physical-size legibility, brightness and glare
remain unmeasured.

## Reproduction

For Ubuntu use the strict compiler commands in the [simulator README](../simulator/README.md).
On Windows use an x64 Native Tools Command Prompt from the repository root
with Python on PATH.

```bat
if not exist simulator\build mkdir simulator\build
cl /nologo /std:c++17 /permissive- /EHsc /W4 /WX /utf-8 /I firmware\include /Fo:simulator\build\telemetry_test.obj /Fe:simulator\build\telemetry_test.exe firmware\test\telemetry_test.cpp
cl /nologo /std:c++17 /permissive- /EHsc /W4 /WX /utf-8 /I firmware\include /Fo:simulator\build\navigation_test.obj /Fe:simulator\build\navigation_test.exe firmware\test\navigation_test.cpp
cl /nologo /std:c++17 /permissive- /EHsc /W4 /WX /utf-8 /I firmware\include /Fo:simulator\build\fixture.obj /Fe:simulator\build\fixture.exe simulator\fixture.cpp
simulator\build\telemetry_test.exe
simulator\build\navigation_test.exe
python simulator\test_render.py
cd simulator
npm ci
set PLAYWRIGHT_BROWSERS_PATH=0
npx playwright install chromium
set SIMULATOR_SCREENSHOTS=build\phase-1b
npm test
```

The host CI workflow uses Ubuntu 24.04, logs the runner-provided GCC and
Python versions, pins Node.js and all referenced actions, and installs
Playwright from the lockfile. It publishes a short-lived HTML report and
browser failure artifacts when the Chromium step fails. The workflow uses
read-only contents permission. Local actionlint 1.7.7 validation passed.

## Outstanding verification

V06 and V07 require a real authorized BLE link, a companion/provider
implementation, locked-phone testing and route playback. Board validation,
USB integration, physical readability, daylight and glove testing, and
vehicle telemetry remain pending. The simulator uses only synthetic data.
Highway exits and complex maneuvers require a protocol extension.
