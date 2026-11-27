# Navigation provider research

Desk review: 2026-11-26. No SDK integrated or real-device experiment performed. Target: personal iOS companion driving a small external ESP32 display, with core telemetry independent of phone/network.

## Findings and decision

| Candidate | Guidance capability | Constraints | Disposition |
|---|---|---|---|
| Apple MapKit | MKDirections returns route/steps; app must implement progression, maneuver normalization, off-route detection and rerouting | Apple Maps terms restrict displaying Map Data without corresponding Apple map and reuse/storage. External arrow-only display permission not established | Hold pending terms review/permission |
| Google Navigation SDK for iOS | Dedicated small-display maneuver feed and navigation-state events; matches UI technically | Published policy limits SDK to commercial applications; billing/API key required; restrictions on persisted content and UI/attribution | Not default for personal project; hold unless permitted |
| MapLibre Navigation iOS + Valhalla | Open-source iOS navigation and OSM-based routing service; potential provider-independent path | Evaluate current iOS SDK maintenance, buildability, routing accuracy, server or permitted hosted service, map/data attribution and licenses | Preferred research spike, not dependency selection |
| GPX route/waypoint guidance | Own/prepared route and phone location; limited off-route behavior | No automatic general routing without engine; clarify GPX maneuver source | Later fallback/enhancement |
| Android notification forwarding | Android-specific mechanisms demonstrated by projects | Not equivalent to iOS notification access; outside current platform scope | Reference only |

Google's technical fit does not override its commercial-use restriction. Apple MapKit is not automatically permitted for a standalone external arrow display. Do not create cloud accounts, buy an SDK, copy provider data, or select a paid dependency in Phase 0. Open-source routing still has operating costs and attribution/data obligations. Review licenses of exact pinned revisions and map data separately. No mandatory cloud for telemetry.

## Sources checked

- [Apple MKDirections](https://developer.apple.com/documentation/mapkit/mkdirections) describes server-generated route directions. [Apple Developer Program License Agreement, Attachment 6](https://developer.apple.com/support/terms/apple-developer-program-license-agreement/) §§2.2–2.5 constrain extraction, display and storage; determine applicability to external accessory before using it.
- [Google turn-by-turn data feed](https://developers.google.com/maps/documentation/navigation/ios-sdk/nav-only-feed) exposes maneuver, distance, state and street information for compact displays, including two-wheel use. [Google policies](https://developers.google.com/maps/documentation/navigation/ios-sdk/policies) states commercial-use-only and restricts storage of street/speed-limit content outside session. [Billing](https://developers.google.com/maps/documentation/navigation/ios-sdk/pricing) bills route generation by requested destination/waypoint, with guidance/reroutes to existing destinations not separately charged; live rates/quotas must be checked if eligible. No cost commitment.
- [MapLibre Navigation iOS](https://github.com/maplibre/maplibre-navigation-ios) describes customizable routing endpoints; [Valhalla](https://github.com/valhalla/valhalla) is MIT-licensed routing software, with [route API](https://valhalla.github.io/valhalla/api/route/api-reference/) providing maneuver data. Verify exact revision, licenses, OSM obligations, hosting limits and update cadence. Do not depend on a public demo endpoint as production infrastructure.
- [Apple Core Bluetooth background processing](https://developer.apple.com/library/archive/documentation/NetworkingInternetWeb/Conceptual/CoreBluetooth_concepts/CoreBluetoothBackgroundProcessingForIOSApps/PerformingTasksWhileYourAppIsInTheBackground.html) and [Core Location background updates](https://developer.apple.com/documentation/corelocation/handling-location-updates-in-the-background) describe background capabilities; neither guarantees continuous arbitrary execution. Real locked-phone test required.
- [ESP32-S3 RF coexistence](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/coexist.html): Wi-Fi and BLE share radio time, so concurrent reliability must be measured.
- [Waveshare board wiki](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.43) and [ESP-IDF examples](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.43/ESP-IDF): candidate display/peripherals and bring-up examples, not verification of delivered revision.
- [LVGL simulator repository](https://github.com/lvgl/lv_port_pc_vscode): candidate host workflow; verify LVGL major version and SDL/toolchain compatibility.

## Prior art (README-level review)

| Project | Evidence / useful pattern | Limitation |
|---|---|---|
| [SteedPilot](https://github.com/deanthecoder/SteedPilot) | MIT listed; circular display, BLE state fixtures, heartbeat and simulator | Documentation is not evidence of validated full iOS navigation; check pinned code and license |
| [Motor_Display](https://github.com/annguyenkhanh/Motor_Display) | README declares MIT; MapKit route compression and display | External MapKit use terms and real-world route reliability unresolved |
| [NavDisplay](https://github.com/nandushajahan/NavDisplay) | README declares MIT; Android accessibility/notification forwarding | Android-specific; not portable to iOS |
| [RoyalEnfieldObd](https://github.com/moca-owner-dev/RoyalEnfieldObd) | MIT listed; Wi-Fi ELM327, mock telemetry and PID decoding | README contradicts itself on CAN/K-line; target ECU compatibility unverified |

No third-party source, asset or protocol copied. Validate LICENSE and third-party dependencies at pinned commit before reuse; retain notices.

## Research experiment

1. Build a provider-free synthetic maneuver stream first. Compare open-source iOS SDK build/install on supported macOS/Xcode and a physical iPhone; personal signing and distribution need owner decision.
2. Evaluate permitted routing endpoint/tiles and OSM attribution; document costs and availability, including loss of mobile data. No live API credentials in repository.
3. Test route progress, missed turns, reroute, GPS degradation, tunnel/screen lock, background permission changes, Bluetooth toggle, app termination, restart and phone absent. A forced-quit app is not guaranteed to restart automatically.
4. Simultaneously run ECU-like Wi-Fi traffic and BLE guidance; record sample age, rejected frames, disconnects, reconnect times and display behavior.
5. Prototype 466 × 466 circular viewport on desktop with mock states. Check arrow, distance and data-loss presentation; no moving interaction required.
6. Record outcome, licensing clearance, platform/toolchain versions, limitations and recommended provider in ADR-002. Issue #7 stays open until review and experiments are complete.
