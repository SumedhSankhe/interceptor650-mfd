# Hardware and bill of materials

## Selected items (2026-11-26)

| Item | Price USD | Purpose |
|---|---:|---|
| Waveshare ESP32-S3 1.43-inch AMOLED | 33.51 | MCU and display |
| Wefomey variable-voltage 3–24 V, 3 A supply | 17.09 | Bench prototyping; not a verified current-limited bench supply |
| BOCEUC miniature SMD grabber clips | 7.99 | Secure probe connections |
| Extended needle multimeter probes | Unknown | Diagnostic probing |

Known subtotal: $58.59 before fourth item, taxes and discounts. Existing AstroAI meter and original Tripper harness available.

## Enclosure

Photo-based estimates, not caliper-verified: outer diameter 54–55 mm; internal opening and original assembly 46–47 mm; depth approximately 25 mm. Central mounting stud and screw bosses reduce usable space. Nominal module body ~46 mm; check actual board variant, maximum width and connector clearance. Do not cut housing until test-fit. Reuse original lens and bezel where practical. Original seal is compromised; weather protection must be restored and tested. Sunlight readability, vibration and heat require validation.

[Waveshare board documentation](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.43). Confirm exact board revision and presence of microSD/RTC before assuming features.

## Ignition-powered design

Use integrated module functions once the delivered revision and capabilities
are confirmed. The final protected supply is ignition-switched. External
circuitry is limited to electrical compatibility and automotive power
protection. This design excludes a main backup battery, supercapacitor,
external shutdown controller and custom shutdown circuitry. RTC backup is
optional and deferred. The software assumes the supply can disappear
without warning and recovers on the next boot. Card, RTC, regulator and
cutoff behavior remain subject to bench verification.

## Deferred items

- Adjustable current limiting for original Tripper powered test.
- Diagnostic adapter/cable: select after ECU and connector identification.
- Protected automotive DC/DC converter: correct input/output, transient suppression, reverse-polarity protection and ignition behavior.
- Fuse/holder, wire, heat shrink, mounting spacers, electronics-safe sealant after design.
- Endurance microSD only if delivered board supports it.

Power the Waveshare board over USB initially. Never connect it directly to a motorcycle 12 V supply.
