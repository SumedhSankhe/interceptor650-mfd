"""Local-only simulator HTTP bridge. Run after building the C++ fixture."""
import argparse
import json
import subprocess
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit, parse_qs
from render import FIXTURE, HERE, render_snapshot, runtime_snapshot


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        url = urlsplit(self.path)
        if url.path == "/":
            self.respond((HERE / "index.html").read_bytes(), "text/html; charset=utf-8")
        elif url.path == "/favicon.ico":
            self.send_response(204)
            self.end_headers()
        elif url.path in ("/snapshot.json", "/display.svg", "/ride.json"):
            params = parse_qs(url.query, keep_blank_values=True)
            value = lambda key, default: params.get(key, [default])[0]
            try:
                if url.path == "/ride.json":
                    # Historical endpoint. The browser uses the combined snapshot endpoint.
                    data = runtime_snapshot(ride=value("scenario", "ignition"))
                else:
                    data = runtime_snapshot(
                        mode=value("mode", "telemetry"), scenario=value("scenario", "normal"),
                        nav=value("nav", "active"), unit=value("unit", "C"),
                        maneuver=value("maneuver", "right"), distance=value("distance", "400"),
                        exit_number=value("exit_number", "2"), age=value("age", "0"),
                        distance_unit=value("distance_unit", "metric"),
                        connection=value("connection", "current"), ride=value("ride", "ignition"),
                        replay=value("replay", "fixtures"), step=value("step", "0"))
            except ValueError:
                self.send_error(400)
                return
            except subprocess.CalledProcessError:
                self.send_error(500)
                return

            if url.path == "/display.svg":
                self.respond(render_snapshot(data).encode(), "image/svg+xml")
            else:
                payload = data["diagnostics"] if url.path == "/ride.json" else {
                    "svg": render_snapshot(data), "diagnostics": data["diagnostics"]}
                self.respond(json.dumps(payload).encode(), "application/json")
        else:
            self.send_error(404)

    def respond(self, data, content_type):
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)


if __name__ == "__main__":
    if not FIXTURE.is_file():
        raise SystemExit("Build the C++ fixture first; see simulator/README.md")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8765, help="loopback port (default: 8765)")
    args = parser.parse_args()
    with ThreadingHTTPServer(("127.0.0.1", args.port), Handler) as server:
        print(f"Open http://127.0.0.1:{server.server_port}", flush=True)
        server.serve_forever()
