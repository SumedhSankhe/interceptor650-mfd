# Roadmap and issue traceability

Phase gates are sequential for deployment; research and host simulation can overlap. See [requirements](requirements.md) and [verification plan](verification.md).

| Phase | Deliverable | Existing issue | Exit gate |
|---|---|---|---|
| 0 | Review engineering baseline and navigation feasibility | #7 | Review PR; provider and hardware experiments remain open |
| 1 | Original Tripper protected bench diagnosis | #1 | V01 |
| 1 | Vendor board USB bring-up | #3 | V03 |
| 1 | Host mock telemetry dashboard | #4 | V02 host evidence recorded |
| 1D | Integrated portable runtime, recovery and ride replay | #4/#6/#7 | V02/V05/V09 host evidence recorded |
| 2 | Identify ECU and supported diagnostic data | #2 | V04 |
| 2 | Protected power, enclosure mounting and sealing | #5 | V08/V10 |
| 2 | Live telemetry and stationary validation | #6 | Applicable V04/V08/V10 before road testing |
| 3 | iOS navigation and hybrid UI | #7 | Provider permission and V05–V08; create implementation issues after spike |
| 4 | Persistent logging, GPS opt-in and analytics | #6 plus future issues | V09; applicable road-readiness gates |

## Completed observations
- [x] Disassemble original Tripper and preserve enclosure, bezel and electronics.
- [x] Record passive PCB measurements and harness continuity.

## Open work
- [ ] Perform protected 12 V bench test and diagnose original fault.
- [ ] If original boots, test vehicle-side power and connector under load.
- [ ] Confirm Waveshare board revision; run vendor USB example.
- [x] Build the host mock telemetry dashboard, stale-data state and integrated application runtime.
- [ ] Identify ECU label and diagnostic connector; establish protocol and supported PIDs.
- [ ] Select and validate diagnostic interface.
- [ ] Test-fit display; design protected power and mounting.
- [ ] Investigate provider permissions, iOS locked-phone operation and BLE/Wi-Fi coexistence.
- [ ] Implement navigation after research and design review.
- [ ] Validate logging, ignition-off behavior and quiescent current.
- [ ] Restore environmental sealing; validate vibration, temperature and sunlight readability.
- [ ] Road-test and compare repeatable thermal baselines.

## Decision gates

Preserve the original PCB until its bench test is complete. Do not purchase an OBD adapter until compatibility is established. No permanent enclosure modification until physical fit is measured. No road use until mechanical and electrical reliability are established. No paid SDK adoption or provider content redistribution without clearance.

The original motivation is measuring engine thermal behavior after exhaust/intake changes and monitoring charging voltage. The regulator/rectifier failure has not been causally linked to those modifications. Evaluate thermal evidence before deciding on fueling changes. No issue is closed by documentation alone.
