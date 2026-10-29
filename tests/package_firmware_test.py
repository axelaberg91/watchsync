#!/usr/bin/env python3
# SPDX-License-Identifier: CC-BY-NC-4.0
# Firmware distribution guard checks by axelaberg91.
import hashlib
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path


def main():
    root = Path(__file__).resolve().parents[1]
    marker = re.search(r'OTA_TARGET\[\] = "([^"]+)"', (root / "watchsync/ota_policy.h").read_text(encoding="utf-8"))[1].encode()
    version = re.search(r'^#define VERSION "([^"]+)"', (root / "watchsync/watchsync.ino").read_text(encoding="utf-8"), re.M)[1]
    # TemporaryDirectory creates and removes only this dedicated test directory.
    with tempfile.TemporaryDirectory(prefix="watchsync-package-check-") as directory:
        work = Path(directory).resolve()
        sketch = work / "watchsync"
        sketch.mkdir()
        image = bytearray(5000)
        image[0:2] = b"\xe9\x04"
        image[23] = 1
        image[32:36] = bytes.fromhex("3254cdab")
        image[300:300 + len(marker)] = marker
        profile = b"Firmware profile: portal-only"
        image[500:500 + len(profile)] = profile
        banner = f"watchsync {version} ready.".encode()
        image[800:800 + len(banner)] = banner
        binary = work / "watchsync.ino.bin"
        options = {"fqbn": "m5stack:esp32:m5stack_atom:PartitionScheme=min_spiffs", "sketchLocation": str(sketch)}

        def save(data):
            binary.write_bytes(data + hashlib.sha256(data).digest())
            (work / "build.options.json").write_text(json.dumps(options), encoding="utf-8")

        def package(expected, text="", commit="a" * 40):
            result = subprocess.run([sys.executable, str(root / "tools/package_firmware.py"), str(binary), str(work / "output"), "--commit", commit], capture_output=True, text=True)
            assert (result.returncode == 0) == expected, result.stdout + result.stderr
            assert text in result.stdout + result.stderr, result.stdout + result.stderr

        save(image)
        package(False, "exact 40-character", "local")
        package(True)
        manifest = json.loads((work / "output/manifest.json").read_text(encoding="utf-8"))
        assert manifest["file"] == f"watchsync-{version}.bin"
        assert (work / "output" / manifest["file"]).is_file()
        assert (work / "output/SHA256SUMS").read_text().strip().endswith("  " + manifest["file"])
        assert manifest["sha256"] == hashlib.sha256(binary.read_bytes()).hexdigest()
        assert manifest["commit"] == "a" * 40
        assert all((work / "output" / name).is_file() for name in ("RELEASE_NOTES.md", "LICENSE.md", "CC-BY-NC-4.0.txt"))
        assert manifest["kind"] == "application-only" and not manifest["containsCompiledWifiCredentials"]
        package(False, "non-empty")
        mismatch = bytearray(image)
        mismatch[800:800 + len(banner)] = b"X" * len(banner)
        save(mismatch)
        package(False, "binary version")
        private_path = bytearray(image)
        leak = b"C:" + b"/" + b"Users" + b"/example/build/source.cpp"
        private_path[1200:1200 + len(leak)] = leak
        save(private_path)
        package(False, "Private user path")
        save(image)
        options["fqbn"] = "m5stack:esp32:m5stack_atom:PartitionScheme=huge_app"
        save(image)
        package(False, "Minimal SPIFFS")
        options["fqbn"] = "m5stack:esp32:m5stack_atom:PartitionScheme=min_spiffs"
        save(image)
        (sketch / "secrets.h").write_text("placeholder", encoding="utf-8")
        package(False, "private credentials")
        compiled = bytearray(image)
        compiled[500:500 + len(profile)] = b"X" * len(profile)
        notice = b"Firmware profile: compiled Wi-Fi credentials"
        compiled[600:600 + len(notice)] = notice
        save(compiled)
        package(False, "portal-only")
        save(image)
        damaged = bytearray(binary.read_bytes())
        damaged[100] ^= 1
        binary.write_bytes(damaged)
        package(False, "SHA-256")
        wrong = bytearray(image)
        wrong[12] = 9
        save(wrong)
        package(False, "classic ESP32")
    print("PASS firmware packaging: portal-only image, checksum, board layout, credential exclusion, corruption and wrong chip")


if __name__ == "__main__":
    main()
