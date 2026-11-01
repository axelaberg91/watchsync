#!/usr/bin/env python3
# SPDX-License-Identifier: CC-BY-NC-4.0
"""Preview the Pages artifact under its configured project path on localhost."""
import argparse
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parents[1]
ARTIFACT = ROOT / 'dist' / 'pages'
BASE = json.loads((ROOT / 'site-config.json').read_text(encoding='utf-8'))['basePath']


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(ARTIFACT), **kwargs)

    def translate_path(self, path):
        pathname = urlsplit(path).path
        if not pathname.startswith(BASE):
            return str(ARTIFACT / '.outside-project-path')
        return super().translate_path('/' + pathname[len(BASE):])

    def send_error(self, code, message=None, explain=None):
        if code == 404:
            page = ARTIFACT / '404.html'
            if page.is_file():
                body = page.read_bytes()
                self.send_response(404)
                self.send_header('Content-Type', 'text/html; charset=utf-8')
                self.send_header('Content-Length', str(len(body)))
                self.end_headers()
                if self.command != 'HEAD':
                    self.wfile.write(body)
                return
        super().send_error(code, message, explain)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, default=8092)
    arguments = parser.parse_args()
    if not (ARTIFACT / 'index.html').is_file():
        parser.error('Build the Pages artifact before starting the preview.')
    print(f'Preview: http://127.0.0.1:{arguments.port}{BASE}', flush=True)
    ThreadingHTTPServer(('127.0.0.1', arguments.port), Handler).serve_forever()
