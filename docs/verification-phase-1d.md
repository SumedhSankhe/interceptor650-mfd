# Phase 1D host verification

| Field | Record |
|---|---|
| Date | 2026-11-26 (Pacific time) |
| Scope | V02, V05 and host V09 for [Issue #4](https://github.com/SumedhSankhe/interceptor650-mfd/issues/4), [Issue #6](https://github.com/SumedhSankhe/interceptor650-mfd/issues/6) and [Issue #7](https://github.com/SumedhSankhe/interceptor650-mfd/issues/7) |
| Revision | `feature/application-runtime`, based on `7092130` |
| Local host | Windows x64 build 26200, Visual Studio 2022 17.11.2, MSVC 14.41.34120 |
| Tools | Python 3.13.5, Node.js 22.20.0, npm 10.9.3, Playwright 1.65.1, headless Chromium 159.0.8154.6 (revision 1255), actionlint 1.7.7 |
| Inputs | Synthetic data, injected monotonic times, delayed worker execution and storage faults |

## Results

| Coverage | Observation |
|---|---|
| V02 · RT-001 | All four strict C++17 suites passed. The runtime preserved independent battery voltage when ECU readings cleared, and active guidance continued when the ECU disconnected. Disconnect and reboot cleared guidance and cached sensor values without reviving them on reconnect. |
| V05 · RT-001 | The runtime rejected duplicate and out-of-order navigation frames and callbacks from an old link generation. The existing binary-decoder suite, wire vectors and timeout boundary coverage passed. |
| V09 · RT-002 | Four records queued while the worker was held. On the next stop the sample dropped, the session remained open, and a sync barrier followed the last accepted sample. Coalescing, large jumps, regressions, missing storage at boot, simultaneous loss of both links, and write/sync errors passed dedicated assertions. Cold boot restored settings, preserved the prior session bytes and allocated session 2. |
| HTTP and visual | Seven Python tests passed, preserving 168 renderer combinations and all three historical SVG fixtures. All 20 Chromium cases passed. Browser checks compared the displayed SVG bytes and both sequence attributes with the combined response, and verified clipping, alignment and responsive controls. |
| Tooling | MSVC AddressSanitizer passed the application assertions. actionlint passed. The hosted Ubuntu workflow runs the same C++ suites, Python and Playwright alongside pinned setup and artifact actions. |

Timeline observations illustrate synthetic outcomes. At step 7, the display
retains roundabout guidance and independent 12.6 V while RPM and oil data
are unavailable. At step 13, diagnostics show queue depth and high-water
of four, one dropped sample, and a retained recording session. At step 17
the new session starts at uptime zero, reports a truncated prior session
and recovers ten complete samples. Cold boot supplies no live readings or
guidance. The desktop inspection found the display labels centered and the
replay controls usable at 280 px without horizontal overflow. The runtime
contains no host or embedded performance measurements.

## Browser captures

Desktop captures used a 1100 × 680 CSS px viewport at device scale 1.
The narrow capture used a 280 × 760 viewport. All are full-page Chromium
screenshots.

- [Normal telemetry](../simulator/screenshots/phase-1d/runtime-telemetry.png) and [normal hybrid](../simulator/screenshots/phase-1d/runtime-hybrid.png)
- [Stall with session retained](../simulator/screenshots/phase-1d/runtime-stall.png)
- [ECU disconnected with active navigation](../simulator/screenshots/phase-1d/runtime-ecu-gap.png) and [phone disconnected with active telemetry](../simulator/screenshots/phase-1d/runtime-phone-gap.png)
- [Saturated recording queue](../simulator/screenshots/phase-1d/runtime-saturation.png) and [combined degraded state](../simulator/screenshots/phase-1d/runtime-degraded.png)
- [Cold boot recovery](../simulator/screenshots/phase-1d/runtime-recovery.png) and [narrow replay controls](../simulator/screenshots/phase-1d/runtime-replay-narrow.png)

## Reproduction

Use the [simulator instructions](../simulator/README.md) for the Ubuntu
build. On Windows run from an x64 Native Tools Command Prompt at the
repository root with Python on PATH.

```bat
if not exist simulator\build mkdir simulator\build
cl /nologo /std:c++17 /permissive- /EHsc /W4 /WX /utf-8 /I firmware\include /Fo:simulator\build\telemetry_test.obj /Fe:simulator\build\telemetry_test.exe firmware\test\telemetry_test.cpp
cl /nologo /std:c++17 /permissive- /EHsc /W4 /WX /utf-8 /I firmware\include /Fo:simulator\build\navigation_test.obj /Fe:simulator\build\navigation_test.exe firmware\test\navigation_test.cpp
cl /nologo /std:c++17 /permissive- /EHsc /W4 /WX /utf-8 /I firmware\include /Fo:simulator\build\recording_test.obj /Fe:simulator\build\recording_test.exe firmware\test\recording_test.cpp
cl /nologo /std:c++17 /permissive- /EHsc /W4 /WX /utf-8 /I firmware\include /Fo:simulator\build\application_test.obj /Fe:simulator\build\application_test.exe firmware\test\application_test.cpp
cl /nologo /std:c++17 /permissive- /EHsc /W4 /WX /utf-8 /I firmware\include /Fo:simulator\build\fixture.obj /Fe:simulator\build\fixture.exe simulator\fixture.cpp
simulator\build\telemetry_test.exe
simulator\build\navigation_test.exe
simulator\build\recording_test.exe
simulator\build\application_test.exe
python simulator\test_render.py
cd simulator
npm ci
set PLAYWRIGHT_BROWSERS_PATH=0
npx playwright install chromium
set SIMULATOR_SCREENSHOTS=build\phase-1d
npm test
```

To repeat the runtime sanitizer check, add `/fsanitize=address /Zi` to
the application-test compile, pass `/link /INCREMENTAL:NO`, and run the
resulting executable with the MSVC AddressSanitizer runtime on PATH.

## Open gates

Physical display/USB, ECU acquisition rate and PID support, authorized BLE
and provider integration, actual scheduler latency, physical clock quality,
ignition cutoff and SD-card behavior remain open. Live media replacement
and cross-media identifier policy are deferred. See the
[runtime contract](runtime.md) for archive scaling, handoffs and durable
sync limits. No hardware issue was closed.
