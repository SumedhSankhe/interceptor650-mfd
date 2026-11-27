# Roadmap

- [x] Disassemble original Tripper and preserve enclosure, bezel and electronics.
- [x] Record passive PCB measurements and harness continuity.
- [ ] Perform protected 12 V bench test and diagnose original fault.
- [ ] If original boots, test vehicle-side power and connector under load.
- [ ] Confirm Waveshare board revision; run vendor USB example.
- [ ] Build mock telemetry dashboard and stale-data state.
- [ ] Identify ECU label and diagnostic connector; establish protocol and supported PIDs.
- [ ] Select and validate diagnostic interface.
- [ ] Test-fit display; design protected power and mounting.
- [ ] Validate logging, ignition-off behavior and quiescent current.
- [ ] Restore environmental sealing; validate vibration, temperature and sunlight readability.
- [ ] Road-test and compare repeatable thermal baselines.

## Decision gates

Preserve the original PCB until its bench test is complete. Do not purchase an OBD adapter until compatibility is established. No permanent enclosure modification until physical fit is measured. No road use until mechanical and electrical reliability are established.

The original motivation is measuring engine thermal behavior after exhaust/intake changes and monitoring charging voltage. The regulator/rectifier failure has not been causally linked to those modifications. Evaluate thermal evidence before deciding on fueling changes.
