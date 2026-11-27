# V02 desktop browser verification

| Field | Value |
|---|---|
| Date | 2026-11-26 (Pacific time) |
| Scope | V02 host simulator, [Issue #4](https://github.com/SumedhSankhe/interceptor650-mfd/issues/4) |
| Revision | `test/simulator-browser-e2e` based on `97881b0`. The validation files and PNGs are in the same change. |
| Host | Windows x64, build 26200, Visual Studio 2022 17.11.2, MSVC 14.41.34120 |
| Test tools | Python 3.13.5, Node.js 22.20.0, npm 10.9.3, Playwright 1.65.1, headless Chromium 159.0.8154.6 (Playwright revision 1255) |
| Inputs | Deterministic C++ mock telemetry and static navigation fixtures served on loopback |
| Result | Host coverage passed. V02 physical display and USB evidence remain open. |

## Results

| Check | Observation |
|---|---|
| Existing C++ model | Both the assertion test and fixture compiled as C++17 with warnings as errors. The test printed `telemetry assertions passed`. |
| Existing Python renderer | Three tests passed, including all 96 selector combinations, invalid inputs, and the three existing SVG snapshots. |
| Browser modes and units | Telemetry, navigation, and hybrid rendered via the selectors. Oil changed from 108 °C to 226 °F and back. Intake changed from 32 °C to 90 °F. Voltage remained 14.2 V across the unit change. |
| Telemetry degradation | Stale and disconnected inputs removed live-looking values and displayed STALE or MISSING. Unsupported oil temperature displayed UNSUPPORTED while preserving voltage. |
| Navigation degradation | Rerouting, stale guidance and phone disconnection removed the 400 m right-turn guidance and displayed the relevant state. |
| Independent state changes | Active guidance survived telemetry degradation. Telemetry survived navigation degradation. The hybrid sequence also exercised simultaneous degradation and independent recovery. |
| Browser and HTTP | The 13 Chromium cases passed. Successful interaction paths produced zero JavaScript exceptions, console errors, failed requests or HTTP error responses. |
| Circular face and layout | Ten states rendered at 466 × 466 CSS px with transparent corners. Chromium's SVG bounds check put the text and graphics inside the face. At a 280 px browser width the square image scaled to 216 px and the controls fit below it. |

Chromium reproduced a missing favicon request with HTTP 404 and a form overflow
past the content boundary at 280 px. The server now answers the optional favicon
request with HTTP 204, and the form minimum width respects the available space.
The native Windows server also rejected an existing `fixture.exe` at startup.
The bridge and renderer now resolve the same platform-specific executable.

## Browser captures and inspection

PNG files are full-page captures from Chromium at device scale 1. Desktop
captures used a 1100 × 680 viewport. The narrow capture used a 280 × 760
viewport and includes the controls below the fold.

- Modes: [telemetry](../simulator/screenshots/browser/telemetry.png), [navigation](../simulator/screenshots/browser/navigation.png), [hybrid](../simulator/screenshots/browser/hybrid.png).
- Telemetry degradation: [stale](../simulator/screenshots/browser/telemetry-stale.png), [unsupported](../simulator/screenshots/browser/telemetry-unsupported.png), [disconnected](../simulator/screenshots/browser/telemetry-disconnected.png).
- Navigation degradation: [rerouting](../simulator/screenshots/browser/navigation-rerouting.png), [stale guidance](../simulator/screenshots/browser/navigation-stale.png), [phone disconnected](../simulator/screenshots/browser/navigation-disconnected.png).
- [Combined degradation](../simulator/screenshots/browser/combined-degraded.png) and [narrow hybrid layout](../simulator/screenshots/browser/narrow-hybrid.png).

Desktop captures show even alignment of the telemetry columns and consistent
centering of the navigation and degraded-state labels. The oil reading,
maneuver, or NO GUIDANCE message dominates each relevant state. White primary
text and muted secondary text are visually distinct against the dark face.
The 280 px capture makes source and quality labels small. Contrast was assessed
visually at captured size. Contrast ratios and physical-size readability
remain unmeasured.

## Reproduce

From the repository root in an x64 Native Tools Command Prompt with Python
on PATH:

```bat
if not exist simulator\build mkdir simulator\build
cl /nologo /std:c++17 /permissive- /EHsc /W4 /WX /utf-8 /I firmware\include /Fo:simulator\build\telemetry_test.obj /Fe:simulator\build\telemetry_test.exe firmware\test\telemetry_test.cpp
simulator\build\telemetry_test.exe
cl /nologo /std:c++17 /permissive- /EHsc /W4 /WX /utf-8 /I firmware\include /Fo:simulator\build\fixture.obj /Fe:simulator\build\fixture.exe simulator\fixture.cpp
python simulator\test_render.py
cd simulator
npm ci
set PLAYWRIGHT_BROWSERS_PATH=0
npx playwright install chromium
set SIMULATOR_SCREENSHOTS=screenshots\browser
npm test
```

Use [the simulator README](../simulator/README.md) for Linux/WSL build commands
and browser runner settings. The recorded run used the separate-server mode with an ephemeral loopback port. In this Windows session the Playwright-managed server did not exit cleanly after the tests.

## Remaining verification

This run used headless Chromium on one host. Other operating systems, browser
engines, and input devices remain untested in this browser suite. Staleness,
rerouting and disconnection are selected fixture states. Real-time radio and
provider behavior, physical 1.43-inch readability, USB/display integration,
and real sensor or road tests remain outside the host evidence.
