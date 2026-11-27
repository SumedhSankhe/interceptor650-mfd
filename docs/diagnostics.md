# Original Tripper diagnostics

## Hardware

Royal Enfield / Visteon Tripper, label 1100494/G, 12.10/25341, manufactured 2023-11-16. Purchased used; never observed booting. Original LCD, PCB, bezel, lens and housing removed without intentional destruction. PCB has six connector pins provisionally labelled A–F: far row A B C, near row D E F, in the original inspection orientation (connector at top, silver capacitor lower left). These are local labels, not manufacturer pin numbers.

## Unpowered measurements (2026-11-26)

Meter: AstroAI AM33D, manual range. Shorted-probe reading reported as 0.03; treat near-zero readings as continuity, not precision resistance. A displayed 1 indicates over-range/OL, not 1 ohm. Probe contact was corrected before final measurements.

| Test | Observed result | Interpretation |
|---|---|---|
| A–D, B–E, C–F, bare PCB | OL | No continuity at tested range |
| Zero-ohm jumper | Near zero | Continuous |
| Component marked 3BZ/12/M | 584 mV forward, OL reverse | Basic diode behavior, exact identity unverified |
| Across 220 uF 20 V capacitor terminals | ~1700–1920 ohms, fluctuating | No obvious hard short; ESR/capacitance untested |
| MICRO GND to capacitor negative | Near zero | Common ground |
| MICRO GND to E | Near zero | Ground pin |
| MICRO GND to A/B/C/D/F | OL | No direct continuity |
| MICRO 3.3 V to GND | ~1400 ohms | No obvious hard short |
| Cap positive to A/B/C | OL | No direct connection established |
| Cap positive to D/E/F | Brief ~55/77/71 then OL | Transient readings; not proof of function |
| Yellow two-pin harness ground to MICRO GND | Near zero | Ground continuity |
| Yellow other terminal to D and F | Near zero to each | Both tied by harness |
| D to F, with harness attached | Near zero | Harness bridges two pins |

Harness: motorcycle-side yellow two-pin plug, Tripper-side three populated wires red/black/red. E is ground; D/F are tied to the other terminal. Positive polarity is inferred from wiring and must be verified before energizing. Harness continuity does not prove power delivery under load. Passive checks have not identified a definitive failure.

## Next bench test

- [ ] Obtain suitable adjustable current limiting; the ordered variable-voltage supply's 3 A rating is not selectable current protection.
- [ ] Verify supply voltage and polarity independently with the meter. Verify connector orientation.
- [ ] Place original electronics on an insulating surface; secure insulated leads with supply off.
- [ ] Connect through the original harness, not crowded PCB pins. Use verified 12 V and conservative current limiting appropriate to the device. Stop if unexpected current, heating or odor.
- [ ] Record input voltage/current, boot state, capacitor rail and labelled MICRO 3.3 V rail.
- [ ] Never inject power at the 3.3 V pad or connect a jump starter directly.
- [ ] If boot succeeds, inspect motorcycle-side fuse, connector engagement, polarity and voltage under load.
- [ ] If no boot, determine whether 3.3 V is present before further regulator or logic/display investigation.

A successful bench boot narrows the fault to the vehicle-side supply/connection or an intermittent condition; it does not prove a specific defective harness. No powered test has been performed yet.
