"""Deterministic host renderer checks; requires compiled fixture."""
import itertools
import unittest
from render import HERE, MODES, SCENARIOS, NAV_STATES, render


class RenderTests(unittest.TestCase):
    def test_all_fixture_combinations(self):
        for mode, scenario, nav, unit in itertools.product(MODES, SCENARIOS, NAV_STATES, ("C", "F")):
            with self.subTest(mode=mode, scenario=scenario, nav=nav, unit=unit):
                svg = render(mode, scenario, nav, unit)
                self.assertIn('viewBox="0 0 466 466"', svg)
                self.assertIn("SIMULATOR · MOCK ONLY", svg)
                self.assertEqual("400 m" in svg, mode != "telemetry" and nav == "active")
                if mode != "navigation":
                    if scenario in ("stale", "disconnected"):
                        self.assertNotIn("14.2V", svg)
                        self.assertNotIn("108°C", svg)
                        self.assertNotIn("226°F", svg)
                        self.assertIn("STALE" if scenario == "stale" else "MISSING", svg)
                    elif scenario == "unsupported":
                        self.assertIn("UNSUPPORTED", svg)
                        self.assertIn("14.2V", svg)
                    else:
                        self.assertIn("108°C" if unit == "C" else "226°F", svg)
                        self.assertIn("14.2V", svg)

    def test_invalid_input(self):
        for args in [("bad",), ("telemetry", "bad"), ("hybrid", "normal", "bad"), ("telemetry", "normal", "active", "K")]:
            with self.assertRaises(ValueError):
                render(*args)

    def test_committed_snapshots(self):
        for name, args in [("telemetry", ("telemetry", "normal", "active")),
                           ("hybrid", ("hybrid", "normal", "active")),
                           ("degraded", ("hybrid", "stale", "disconnected"))]:
            self.assertEqual(render(*args), (HERE / "screenshots" / (name + ".svg")).read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
