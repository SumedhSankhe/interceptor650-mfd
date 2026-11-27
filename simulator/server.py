"""Local-only simulator HTTP bridge. Run after building the C++ fixture."""
import argparse
import subprocess
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit, parse_qs
from render import FIXTURE, HERE, render


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        url = urlsplit(self.path)
        if url.path == "/":
            self.respond((HERE / "index.html").read_bytes(), "text/html; charset=utf-8")
        elif url.path == "/favicon.ico":
            self.send_response(204)
            self.end_headers()
        elif url.path == "/ride.json":
            scenario = parse_qs(url.query, keep_blank_values=True).get("scenario", ["ignition"])[0]
            result = subprocess.run([str(FIXTURE), "--ride", scenario], capture_output=True)
            if result.returncode == 0:
                self.respond(result.stdout, "application/json")
            else:
                self.send_error(400 if result.returncode == 2 else 500)
        elif url.path == "/display.svg":
            params = parse_qs(url.query, keep_blank_values=True)
            try:
                svg = render(*(params.get(k, [v])[0] for k, v in zip(
                    ("mode", "scenario", "nav", "unit", "maneuver", "distance",
                     "exit_number", "age", "distance_unit", "connection"),
                    ("telemetry", "normal", "active", "C", "right", "400", "2", "0", "metric", "current"))))
            except ValueError:
                self.send_error(400)
            else:
                self.respond(svg.encode(), "image/svg+xml")
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
