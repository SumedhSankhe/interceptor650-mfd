"""Deterministic host renderer checks; requires compiled fixture."""
import itertools
import json
import threading
import unittest
from http.server import ThreadingHTTPServer
from urllib.error import HTTPError
from urllib.request import urlopen

from server import Handler
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

        for options in ({"maneuver": "unknown"}, {"distance": "-1"}, {"distance": "1e3"},
                        {"distance": "4294967296"}, {"exit_number": 256}, {"age": ""},
                        {"distance_unit": "bad"}, {"connection": "bad"}):
            with self.subTest(**options), self.assertRaises(ValueError):
                render(**options)

    def test_navigation_snapshot(self):
        self.assertIn("ROUNDABOUT · EXIT 3", render("hybrid", maneuver="roundabout", exit_number=3))
        self.assertIn("0 m", render("navigation", distance=0, age=2999))
        self.assertIn("STALE", render("navigation", age=3000))
        self.assertIn("ARRIVED", render("navigation", nav="arrived", age=10000))
        self.assertNotIn("400 m", render("navigation", connection="waiting"))
        self.assertIn("400 m", render("navigation", connection="reconnected"))
        self.assertIn("1310 ft", render("hybrid", unit="C", distance_unit="imperial"))

    def test_committed_snapshots(self):
        for name, args in [("telemetry", ("telemetry", "normal", "active")),
                           ("hybrid", ("hybrid", "normal", "active")),
                           ("degraded", ("hybrid", "stale", "disconnected"))]:
            self.assertEqual(render(*args), (HERE / "screenshots" / (name + ".svg")).read_text(encoding="utf-8"))


class RideDiagnosticsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        cls.server_thread = threading.Thread(target=cls.server.serve_forever)
        cls.server_thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server_thread.join()
        cls.server.server_close()

    def diagnostics(self, query):
        with urlopen(f"http://127.0.0.1:{self.server.server_port}/ride.json?{query}", timeout=5) as response:
            self.assertEqual(response.headers.get_content_type(), "application/json")
            self.assertEqual(response.headers["Cache-Control"], "no-store")
            return json.load(response)

    def test_ride_lifecycle(self):
        stopped = self.diagnostics("scenario=ignition")
        self.assertEqual((stopped["rpm"], stopped["engine"], stopped["session"]), (0, "stopped", 1))
        stall = self.diagnostics("scenario=stall")
        self.assertEqual((stall["session"], stall["sync_requests"]), (1, 1))
        loss = self.diagnostics("scenario=ecu-loss")
        self.assertEqual((loss["rpm"], loss["engine"], loss["session"]), (None, "unknown", 1))
        reboot = self.diagnostics("scenario=power-loss")
        self.assertEqual((reboot["session"], reboot["recovered_records"]), (2, 2))
        self.assertIn("Truncated tail", reboot["recovery"])
        storage = self.diagnostics("scenario=storage-failure")
        self.assertEqual((storage["recording"], storage["last_error"]), ("degraded", "full"))
        resumed = self.diagnostics("scenario=storage-recovery")
        self.assertEqual((resumed["recording"], resumed["session"]), ("recording", 1))

    def test_unknown_ride_scenario(self):
        for query in ("scenario=bad", "scenario="):
            with self.subTest(query=query), self.assertRaises(HTTPError) as response:
                self.diagnostics(query)
            self.assertEqual(response.exception.code, 400)
            response.exception.close()


if __name__ == "__main__":
    unittest.main()
