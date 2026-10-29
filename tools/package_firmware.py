#!/usr/bin/env python3
# SPDX-License-Identifier: CC-BY-NC-4.0
# Credential-free firmware packaging by axelaberg91.
"""Package an Atom Lite OTA application binary, never a merged flash image."""
import argparse
import hashlib
import json
import re
import shutil
from pathlib import Path


def validate_image(path, root):
    """Validate the target and staging evidence before exporting any bytes."""
    version = re.search(r'^#define VERSION "([^"]+)"', (root / "watchsync/watchsync.ino").read_text(encoding="utf-8"), re.M)[1]
    marker = re.search(r'OTA_TARGET\[\] = "([^"]+)"', (root / "watchsync/ota_policy.h").read_text(encoding="utf-8"))[1].encode()
    binary = path.read_bytes()
    if not 4096 <= len(binary) <= 0x1E0000:
        raise SystemExit("Application image does not fit an Atom Lite OTA slot.")
    if binary[0] != 0xE9 or binary[12:14] != b"\0\0" or binary[23] != 1 or binary[32:36] != bytes.fromhex("3254cdab"):
        raise SystemExit("Expected a classic ESP32 application image with an appended SHA-256 hash.")
    if marker not in binary or hashlib.sha256(binary[:-32]).digest() != binary[-32:]:
        raise SystemExit("Image target marker or embedded SHA-256 hash is invalid.")
    if b"Firmware profile: portal-only" not in binary or b"Firmware profile: compiled Wi-Fi credentials" in binary:
        raise SystemExit("Only a portal-only firmware image may be shared.")
    if f"watchsync {version} ready.".encode() not in binary:
        raise SystemExit("The binary version does not match the source VERSION.")
    # __FILE__ and assert strings can leak the workstation identity even in a
    # stripped application. Prefix-map the source, core and library roots.
    if re.search(rb'(?i)(?:[A-Z]:[\\/](?:Users|Documents)[\\/]|/(?:Users|home)/)', binary):
        raise SystemExit("Private user path in firmware image; rebuild with source-prefix mappings.")
    options_path = path.parent / "build.options.json"
    if not options_path.exists():
        raise SystemExit("Missing build.options.json; cannot verify credential-free build staging.")
    options = json.loads(options_path.read_text(encoding="utf-8"))
    if options.get("fqbn") != "m5stack:esp32:m5stack_atom:PartitionScheme=min_spiffs":
        raise SystemExit("Expected the M5Stack Atom target with Minimal SPIFFS OTA partitions.")
    sketch = options.get("sketchLocation")
    if not sketch:
        raise SystemExit("Cannot verify the build's credential-free sketch location.")
    if any((Path(sketch) / name).exists() for name in ("secret.h", "secrets.h")):
        raise SystemExit("Refusing to package a build from a sketch containing private credentials.")
    return version, binary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--commit", required=True, help="Exact 40-character source commit.")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    if not re.fullmatch(r"[0-9a-f]{40}", args.commit):
        raise SystemExit("Expected an exact 40-character source commit.")
    version, binary = validate_image(args.binary, root)
    notices = {
        "RELEASE_NOTES.md": root / "docs/releases" / f"{version}.md",
        "LICENSE.md": root / "LICENSE.md",
        "CC-BY-NC-4.0.txt": root / "LICENSES/CC-BY-NC-4.0.txt",
    }
    if any(not source.is_file() for source in notices.values()):
        raise SystemExit("Missing release notes or licensing notice; package must be self-contained.")
    digest = hashlib.sha256(binary).hexdigest()
    filename = f"watchsync-{version}.bin"
    if args.output.exists() and any(args.output.iterdir()):
        raise SystemExit("Refusing to replace a non-empty firmware package. Use a new version directory.")
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / filename).write_bytes(binary)
    (args.output / "SHA256SUMS").write_text(f"{digest}  {filename}\n", encoding="utf-8")
    manifest = {"version": version, "target": "M5 Atom Lite (classic ESP32), GPIO25", "partitionScheme": "min_spiffs",
                "core": "M5Stack ESP32 3.3.10", "commit": args.commit, "size": len(binary), "sha256": digest,
                "file": filename, "kind": "application-only", "containsCompiledWifiCredentials": False}
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    for filename, source in notices.items():
        shutil.copyfile(source, args.output / filename)
    print(f"Packaged watchsync {version}: {len(binary)} bytes; SHA-256 {digest}")


if __name__ == "__main__":
    main()
