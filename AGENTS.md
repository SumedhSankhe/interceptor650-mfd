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

## Session continuity

- At the start of each agent session, read [docs/PROJECT_STATE.md](docs/PROJECT_STATE.md) and [docs/HANDOFF.md](docs/HANDOFF.md). Reconcile the dated checkpoint against current main, open PRs, issues and the relevant specifications before acting.
- Preserve the current milestone and feature freeze unless the owner authorizes a change. Use feature branches and reviewed PRs for implementation. Do not merge implementation PRs without explicit owner approval.
- Update the checkpoint after merged milestones, material decisions or new blockers; record evidence in the authoritative diagnostic or verification documents. Keep the shared record technical and free of private chat history.
