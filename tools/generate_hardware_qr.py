#!/usr/bin/env python3
# SPDX-License-Identifier: CC-BY-NC-4.0
"""Regenerate the enclosure QR cells using qrcode 8.2 and a four-module border."""
import argparse
from importlib.metadata import version
import json
from pathlib import Path
import re
from urllib.parse import urlsplit

import qrcode


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", required=True)
    parser.add_argument("--model", type=Path, default=Path(__file__).resolve().parents[1] / "hardware/watchsync-enclosure.scad")
    parser.add_argument("--check", action="store_true", help="Check cells without modifying the model")
    args = parser.parse_args()
    target = urlsplit(args.url)
    if target.scheme != "https" or not target.netloc or target.username or target.password:
        parser.error("Use an absolute HTTPS destination without embedded credentials")
    if version("qrcode") != "8.2":
        parser.error("Install qrcode==8.2 for reproducible QR generation")
    code = qrcode.QRCode(error_correction=qrcode.constants.ERROR_CORRECT_M, border=4)
    code.add_data(args.url)
    code.make(fit=True)
    matrix = code.get_matrix()
    cells = [[x, y] for y, row in enumerate(matrix) for x, dark in enumerate(row) if dark]
    original = args.model.read_text(encoding="utf-8")
    updated = original
    replacements = {
        r"// QR target: [^\n]+": "// QR target: " + args.url,
        r"qr_modules = \d+;": f"qr_modules = {len(matrix)};",
        r"qr_cells = \[.*?\];": "qr_cells = " + json.dumps(cells, separators=(",", ":")) + ";",
    }
    for pattern, replacement in replacements.items():
        updated, count = re.subn(pattern, lambda match: replacement, updated)
        if count != 1:
            parser.error("Expected exactly one QR definition for each model field")
    if args.check and updated != original:
        parser.error("QR cells do not match the requested destination")
    if not args.check:
        args.model.write_text(updated, encoding="utf-8", newline="\n")
    print(f"QR definition verified: {len(matrix)} modules including the quiet zone")


if __name__ == "__main__":
    main()
