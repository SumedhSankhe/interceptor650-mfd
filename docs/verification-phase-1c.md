# Phase 1C host verification

| Field | Record |
|---|---|
| Date | 2026-11-26 (Pacific time) |
| Scope | V02 and host portion of V09 for [Issue #6](https://github.com/SumedhSankhe/interceptor650-mfd/issues/6) |
| Revision | `feature/ride-lifecycle-persistence`, based on `e2b0b4b` |
| Local host | Windows x64 build 26200, Visual Studio 2022 17.11.2, MSVC 14.41.34120 |
| Tools | Python 3.13.5, Node.js 22.20.0, npm 10.9.3, Playwright 1.65.1, headless Chromium 159.0.8154.6 (revision 1255), actionlint 1.7.7 |
| Inputs | Synthetic RPM and telemetry, injected clock, deterministic storage faults and temporary host files |

## Results

| Requirements | Evidence |
|---|---|
| TEL-004, ENG-001, LOG-003 | Strict C++17 telemetry, navigation and recording assertion suites passed. Coverage includes zero versus missing, stale, unsupported and error RPM, invalid numeric inputs, stop/restart, kill switch, ECU gaps, independent voltage and absence of phone connectivity. Session identity remains constant while powered. |
| LOG-001, NFR-005, V09 host | Assertions passed for one-second sampling, stop-triggered sync, five-second periodic sync, missing/full/unavailable media, write and sync retry, checksum/field damage, and truncation at every byte offset in representative records. The host adapter found the valid prefix, preserved old files byte-for-byte and allocated a new session after reboot. |
| LOG-004 | Assertions passed for writing settings only on a change, reloading a successful replacement, retaining current preferences after a failed write, and recovering defaults from absent, truncated, malformed and unsupported data. |
| V02, UI-001/002, NFR-001/003 | Six Python tests passed, including 168 existing renderer combinations, the three historical SVG snapshots, and the ride diagnostics endpoint. All 18 Chromium cases passed, including all supported maneuvers, responsive/circular bounds and the new ride controls. |
| CI | actionlint passed locally. The Ubuntu job builds all three C++ suites with warnings as errors, then runs Python and Chromium. Tool versions are logged with each hosted run. |

An observed engine stop triggered an immediate sample and sync request but
left the session open. Loss of ECU data yielded unknown state and retained
independently available voltage. During simulated reboot the diagnostics
reported two recoverable samples from the previous session, a truncated
tail and a fresh session ID. Fault injection and host recovery establish
software behavior only. The [recording format and lifecycle](protocol/ride-recording.md)
describe the file-sync boundary and recovery rules.

## Browser evidence

Five full-page screenshots were captured by Chromium at 1100 × 680 CSS px
with device scale 1 and reviewed. The display keeps the 466 × 466 face.
The roundabout exit remains centered inside the counterclockwise glyph,
and the diagnostics stay below the display. Long recovery text wraps within
the card.

- [Counterclockwise roundabout](../simulator/screenshots/phase-1c/maneuver-roundabout.png)
- [Stall with the same recording session](../simulator/screenshots/phase-1c/ride-stall.png)
- [Storage full with hybrid display available](../simulator/screenshots/phase-1c/ride-storage-full.png)
- [Recovery after an interrupted write](../simulator/screenshots/phase-1c/ride-power-recovery.png)
- [Recording without a phone](../simulator/screenshots/phase-1c/ride-without-phone.png)

## Reproduction

On Ubuntu follow the [host build instructions](../simulator/README.md).
On Windows open an x64 Native Tools Command Prompt at the repository root
with Python on PATH.

```bat
if not exist simulator\build mkdir simulator\build
cl /nologo /std:c++17 /permissive- /EHsc /W4 /WX /utf-8 /I firmware\include /Fo:simulator\build\telemetry_test.obj /Fe:simulator\build\telemetry_test.exe firmware\test\telemetry_test.cpp
cl /nologo /std:c++17 /permissive- /EHsc /W4 /WX /utf-8 /I firmware\include /Fo:simulator\build\navigation_test.obj /Fe:simulator\build\navigation_test.exe firmware\test\navigation_test.cpp
cl /nologo /std:c++17 /permissive- /EHsc /W4 /WX /utf-8 /I firmware\include /Fo:simulator\build\recording_test.obj /Fe:simulator\build\recording_test.exe firmware\test\recording_test.cpp
cl /nologo /std:c++17 /permissive- /EHsc /W4 /WX /utf-8 /I firmware\include /Fo:simulator\build\fixture.obj /Fe:simulator\build\fixture.exe simulator\fixture.cpp
simulator\build\telemetry_test.exe
simulator\build\navigation_test.exe
simulator\build\recording_test.exe
python simulator\test_render.py
cd simulator
npm ci
set PLAYWRIGHT_BROWSERS_PATH=0
npx playwright install chromium
set SIMULATOR_SCREENSHOTS=build\phase-1c
npm test
```

## Open physical gates

V02 physical display and V09 on the verified board, card, filesystem and
power interface remain open. Validate actual cutoff and brownout behavior,
RTC/clock quality, storage endurance, and firmware scheduling under load.
PID 010C compatibility, vehicle voltage sources and the protected
ignition-switched supply require independent evidence. Optional RTC backup
remains deferred. This host work provides no physical SD-card durability or
data-loss bound.
