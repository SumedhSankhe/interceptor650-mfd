# Development guidance

- Treat measurements, hypotheses and verified facts separately. Preserve raw readings and dates in diagnostic records.
- Do not invent pinouts, ECU protocols, PIDs, board features, measured temperatures or electrical limits.
- Keep diagnostic communication read-only. No ECU writes, actuator commands or safety-system modifications.
- Start with exact vendor board documentation and a confirmed revision; avoid speculative PlatformIO configurations.
- Keep telemetry acquisition independent of rendering and logging; include a mock provider and explicit stale/missing state.
- Use explicit units, clear identifiers and testable interfaces. Record validation commands and results; never claim tests passed unless run.
- Preserve original motorcycle instruments and electronics. Use protected power, validated mounting and sealing.
- Keep personal identifiers, VINs, credentials and private information out of this public repository.
- No license has been selected. Do not add one without owner approval.
