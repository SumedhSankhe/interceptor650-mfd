# Interceptor 650 MFD

An independent ESP32-based auxiliary multifunction display for the 2023 US-spec Royal Enfield Interceptor 650, reusing the original Tripper enclosure.

## Objectives
- Display engine oil temperature, intake air temperature and charging voltage, subject to ECU compatibility.
- Support optional ride telemetry logging.
- Preserve the original instrument cluster and mounting hardware.

## Current status
- Original Tripper teardown and passive electrical diagnostics complete; root cause remains unknown.
- Protected 12 V bench test pending.
- Waveshare ESP32-S3 1.43-inch AMOLED selected; physical fit and board revision pending verification.
- ECU protocol and supported diagnostic parameters unverified. OBD adapter purchase deferred.

## Documentation
- [Diagnostics](docs/diagnostics.md)
- [Roadmap](docs/roadmap.md)
- [Hardware and BOM](hardware/README.md)
- [Firmware architecture](firmware/README.md)
- [Development conventions](AGENTS.md)

This is an experimental auxiliary instrument, not an OEM or safety-critical device. No redistribution license has been selected yet.
