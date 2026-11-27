"""Local-only simulator HTTP bridge. Run after building the C++ fixture."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit, parse_qs
from render import HERE, render


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        url = urlsplit(self.path)
        if url.path == "/":
            self.respond((HERE / "index.html").read_bytes(), "text/html; charset=utf-8")
        elif url.path == "/display.svg":
            params = parse_qs(url.query)
            try:
                svg = render(*(params.get(k, [v])[0] for k, v in zip(
                    ("mode", "scenario", "nav", "unit"), ("telemetry", "normal", "active", "C"))))
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
    if not (HERE / "build" / "fixture").is_file():
        raise SystemExit("Build the C++ fixture first; see simulator/README.md")
    print("Open http://127.0.0.1:8765", flush=True)
    ThreadingHTTPServer(("127.0.0.1", 8765), Handler).serve_forever()
