"""Host-only SVG renderer. No external services or live inputs."""
import json
import os
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
FIXTURE = HERE / "build" / ("fixture.exe" if os.name == "nt" else "fixture")
MODES = ("telemetry", "navigation", "hybrid")
SCENARIOS = ("normal", "stale", "unsupported", "disconnected")
NAV_STATES = ("idle", "active", "rerouting", "arrived", "unavailable", "disconnected", "stale")
MANEUVERS = ("unknown", "straight", "left", "right", "uturn", "roundabout",
             "slight-left", "slight-right", "keep-left", "keep-right")
CONNECTIONS = ("current", "waiting", "reconnected")
MANEUVER_LABELS = ("UNKNOWN", "STRAIGHT AHEAD", "TURN LEFT", "TURN RIGHT", "U-TURN",
                   "ROUNDABOUT", "SLIGHT LEFT", "SLIGHT RIGHT", "KEEP LEFT", "KEEP RIGHT")
STRAIGHT = "M233 195 V110 M211 132 L233 110 L255 132"
TURN = "M197 195 V145 Q197 133 209 133 H280 M258 111 L280 133 L258 155"
UTURN = "M194 195 V143 A39 39 0 0 1 272 143 V185 M252 165 L272 185 L292 165"
ROUNDABOUT = "M233 199 V186 A32 32 0 1 0 210 132 L174 103 M199 105 L174 103 L178 128"
SLIGHT = "M200 195 V171 Q200 156 211 145 L256 100 M230 100 H256 V126"
KEEP = "M215 195 V176 Q215 165 224 155 L252 127 Q258 121 258 111 V101 M238 121 L258 101 L278 121 M215 176 V111"
GLYPHS = ("", STRAIGHT, TURN, TURN, UTURN, ROUNDABOUT, SLIGHT, SLIGHT, KEEP, KEEP)


def unsigned(value, maximum):
    if isinstance(value, str):
        if not (value.isascii() and value.isdecimal()) or len(value) > 10:
            raise ValueError("Expected unsigned decimal")
    elif type(value) is not int:
        raise ValueError("Expected integer")
    number = int(value)
    if not 0 <= number <= maximum:
        raise ValueError("Number out of range")
    return number


def distance_label(meters, distance_unit):
    if distance_unit == "imperial":
        feet = meters / 0.3048
        if feet < 5280:
            return f"{round(feet) if feet < 10 else round(feet / 10) * 10} ft"
        miles = feet / 5280
        return f"{miles:.1f} mi" if miles < 100 else f"{miles:.0f} mi"
    if meters < 1000:
        return f"{meters} m"
    return f"{meters / 1000:.1f} km" if meters < 100000 else f"{meters / 1000:.0f} km"


def render(mode="telemetry", scenario="normal", nav="active", unit="C", maneuver="right",
           distance=400, exit_number=2, age=0, distance_unit="metric", connection="current"):
    if (mode not in MODES or scenario not in SCENARIOS or nav not in NAV_STATES or unit not in ("C", "F")
            or maneuver not in MANEUVERS[1:] or distance_unit not in ("metric", "imperial")
            or connection not in CONNECTIONS):
        raise ValueError("Invalid fixture selection")
    inputs = (NAV_STATES.index(nav), MANEUVERS.index(maneuver), unsigned(distance, 0xffffffff),
              unsigned(exit_number, 255), unsigned(age, 0xffffffff), CONNECTIONS.index(connection))
    data = json.loads(subprocess.check_output(
        [str(FIXTURE), scenario, unit, *map(str, inputs)], text=True))
    oil, intake, voltage = data["telemetry"]
    navigation = data["navigation"]
    state = NAV_STATES[navigation["state"]]
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
        if state == "active":
            kind = navigation["maneuver"]
            transform = ' transform="translate(466 0) scale(-1 1)"' if kind in (2, 6, 8) else ""
            parts.append(f'<path d="{GLYPHS[kind]}"{transform} fill="none" stroke="#edf2f7" stroke-width="13" stroke-linecap="round" stroke-linejoin="round"/>')
            exit_value = navigation["exit_number"]
            if exit_value is not None:
                label(233, 162, exit_value, 20, "#edf2f7", 600)
            label(233, 244, distance_label(navigation["distance_m"], distance_unit), 44, "#edf2f7", 600)
            caption = MANEUVER_LABELS[kind]
            if exit_value is not None:
                caption += f" · EXIT {exit_value}"
            label(233, 270, caption, 16)
        elif state == "arrived":
            label(233, 165, "ARRIVED", 27, "#edf2f7", 600)
        else:
            label(233, 165, "NO GUIDANCE", 27, "#edf2f7", 600)
            label(233, 201, state.upper(), 19)
        if mode == "hybrid":
            line(286)
            metric(147, 353, "OIL TEMP", oil, "°" + unit, 32)
            metric(319, 353, "VOLTAGE", voltage, "V", 32)
        else:
            label(233, 343, "NAVIGATION FIXTURE", 15)
    label(233, 410, mode.upper(), 15, "#8cc7eb", 600)
    parts.append('</g></svg>')
    return "\n".join(parts) + "\n"
