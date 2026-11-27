# Firmware architecture

## Target

Standalone ESP32-S3 round AMOLED display in original Tripper housing. No phone or Raspberry Pi required during operation. Original analogue instruments remain unchanged.

## Proposed stack

PlatformIO, C++, LVGL, FreeRTOS where appropriate. Begin with the exact vendor board example and confirmed pin map. No board configuration or compilable firmware is committed until the hardware revision is verified.

Separate telemetry providers, display rendering, connection state and logging. Mock provider first; real diagnostic transport only after validation. Mark stale or missing data explicitly; never show simulated data as live. Favor legible large numerals and conservative display refresh rates.

## Data sources (candidates, not verified)

| Metric | Candidate request | Caveat |
|---|---|---|
| Oil temperature | 015C | Availability unverified |
| Intake air temperature | 010F | Availability unverified |
| ECU module voltage | 0142 | Availability unverified |
| Adapter voltage | ATRV | Measures adapter supply, not necessarily battery terminals |
| RPM | 010C | Optional |
| MAP | 010B | Optional |
| Engine load | 0104 | Optional |

[RoyalEnfieldObd](https://github.com/moca-owner-dev/RoyalEnfieldObd) is an older Interceptor example; compatibility with 2023 US-spec motorcycle is not established. Identify ECU manufacturer/part number, connector, wiring, protocol and actual supported requests first. Prior discussion noted conflicting CAN/K-line descriptions and possible Bosch/Continental variants. Avoid generic adapter purchase until verified. ESP32-S3 supports Wi-Fi and BLE, not Bluetooth Classic. Consider a verified wired interface or documented Wi-Fi/BLE transport. Use read-only diagnostic requests, no ECU writes or actuator commands.

## Validation

Compare reported voltage against a multimeter at the battery; distinguish adapter and ECU supply. Verify temperature units and plausibility. Log timestamps only with confirmed clock/storage support. Handle power loss safely and ensure no parasitic drain when parked. Validate direct sunlight readability before riding. Auxiliary display is not safety-critical.
