"""Host-only SVG renderer. No external services or live inputs."""
import json
import os
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
FIXTURE = HERE / "build" / ("fixture.exe" if os.name == "nt" else "fixture")
MODES = ("telemetry", "navigation", "hybrid")
SCENARIOS = ("normal", "stale", "unsupported", "disconnected")
NAV_STATES = ("active", "rerouting", "disconnected", "stale")


def readings(scenario, unit):
    return json.loads(subprocess.check_output([str(FIXTURE), scenario, unit], text=True))


def render(mode="telemetry", scenario="normal", nav="active", unit="C"):
    if mode not in MODES or scenario not in SCENARIOS or nav not in NAV_STATES or unit not in ("C", "F"):
        raise ValueError("Invalid fixture selection")
    oil, intake, voltage = readings(scenario, unit)
    parts = ['<svg xmlns="http://www.w3.org/2000/svg" width="466" height="466" viewBox="0 0 466 466" role="img" aria-label="Simulated motorcycle display">',
             '<circle cx="233" cy="233" r="232" fill="#090d12" stroke="#55616d" stroke-width="2"/>',
             '<g font-family="Arial,sans-serif" text-anchor="middle" fill="#edf2f7">']

    def label(x, y, value, size=17, color="#a9b6c3", weight=400):
        parts.append(f'<text x="{x}" y="{y}" font-size="{size}" fill="{color}" font-weight="{weight}">{value}</text>')

    def line(y):
        parts.append(f'<path d="M98 {y} H368" stroke="#34404b" stroke-width="1"/>')

    def metric(x, y, name, data, suffix, size=40):
        label(x, y - size - 9, name, 15)
        value = data["value"]
        shown = "—" if value is None else (f"{value:.1f}" if suffix == "V" else f"{value:.0f}") + suffix
        label(x, y, shown, size, "#edf2f7" if value is not None else "#a9b6c3", 600)
        label(x, y + 24, data["quality"] if value is None else data["source"], 13, "#a9b6c3")

    label(233, 65, "SIMULATOR · MOCK ONLY", 15, "#8cc7eb", 600)
    if mode == "telemetry":
        metric(233, 222, "OIL TEMP", oil, "°" + unit, 72)
        line(270)
        metric(147, 342, "INTAKE", intake, "°" + unit, 32)
        metric(319, 342, "VOLTAGE", voltage, "V", 32)
    else:
        if nav == "active":
            parts.append('<path d="M197 195 V145 Q197 133 209 133 H280 M258 111 L280 133 L258 155" fill="none" stroke="#edf2f7" stroke-width="13" stroke-linecap="round" stroke-linejoin="round"/>')
            label(233, 244, "400 m", 44, "#edf2f7", 600)
            label(233, 270, "TURN RIGHT", 16)
        else:
            label(233, 165, "NO GUIDANCE", 27, "#edf2f7", 600)
            label(233, 201, nav.upper(), 19)
        if mode == "hybrid":
            line(286)
            metric(147, 353, "OIL TEMP", oil, "°" + unit, 32)
            metric(319, 353, "VOLTAGE", voltage, "V", 32)
        else:
            label(233, 343, "NAVIGATION FIXTURE", 15)
    label(233, 410, mode.upper(), 15, "#8cc7eb", 600)
    parts.append('</g></svg>')
    return "\n".join(parts) + "\n"
