# Agent handoff and working agreement

GitHub is the shared, durable engineering record. ChatGPT Project chats and Codex threads are useful working context but are not the authority for implementation status. Do not depend on model memory or assume access to the other agent's conversation.

## Roles

- Owner: product priorities, physical tests and measurements, engineering approvals.
- Codex: implementation on feature branches, local builds/tests, screenshots, documentation and pull requests. Do not merge implementation PRs or close hardware-dependent issues without explicit owner approval.
- ChatGPT: architecture, sprint specifications, independent PR review, source and genuine screenshot inspection, hosted CI verification and squash merges of owner-approved PRs. Cannot assume access to the owner's local terminal or hardware.

## Start each session

1. Read root AGENTS.md, [PROJECT_STATE.md](PROJECT_STATE.md), this handoff, relevant requirements/ADRs and verification records.
2. Inspect live main, branch/worktree status, open PRs and issues. Preserve uncommitted user work. Reconcile any mismatch with the dated checkpoint.
3. State the next authorized action and current blockers. Do not start a new major feature sprint during the hardware-validation freeze.
4. Make the smallest justified changes. Maintain portable domain logic and existing interfaces; document hardware dependencies instead of guessing.
5. Run relevant tests and document exact commands/results, limitations and authentic evidence. Never imply physical validation from host tests.
6. For implementation, open a PR and await review/approval. After the merge or a material change, refresh PROJECT_STATE.md and the relevant authoritative docs. Do not duplicate entire chat transcripts.

## Windows simulator troubleshooting

The owner successfully accessed the simulator from an independent Windows PowerShell process after an earlier server appeared to listen on loopback port 8765 but TCP connections timed out. The original server used a repository-local Python executable while the independent terminal used the Microsoft Store Python alias. Same-process Python loopback worked. Starting a fresh server from the independent terminal on an alternate port resolved access. Exact root cause (including sandbox isolation) was not established. Do not change firewall policy or application code solely on this evidence.

From a clean, up-to-date checkout, rebuild the native fixture using the current [simulator README](../simulator/README.md) and [verification commands](verification-phase-1d.md). Run in independent PowerShell:

```powershell
& "$env:LOCALAPPDATA\Microsoft\WindowsApps\python.exe" .\simulator\server.py --port 8888
```

Open http://127.0.0.1:8888 and keep the terminal running. If the alias is unavailable, use a verified local Python installation. The server is loopback-only. Browser tests use their own managed server; successful tests do not prove a separately launched server remains accessible. Do not include local usernames/paths, credentials, VINs or unrelated personal data in this public repository.

The integrated simulator has a deterministic 18-step timeline (steps 0–17), unified snapshot/diagnostics, telemetry/navigation/hybrid display and fault injection. Use it for interface review before porting to LVGL; a desktop viewport is not proof of physical-size readability.

## Resume prompt

"Resume the Interceptor 650 MFD project. Read AGENTS.md, docs/PROJECT_STATE.md and docs/HANDOFF.md. Reconcile with current main, open pull requests, issues, architecture decisions and verification evidence. Summarize the current state and next authorized engineering action. Do not assume hardware-dependent validation is complete."

This handoff captures technical state and working conventions, not private chat history or the contents of local Codex memory.
