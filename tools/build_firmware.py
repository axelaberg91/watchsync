#!/usr/bin/env python3
# SPDX-License-Identifier: CC-BY-NC-4.0
# Original distribution tooling by axelaberg91.
"""Build the supported Atom Lite profile in isolated, credential-free staging."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
GIT = ["git", "-c", f"safe.directory={ROOT.as_posix()}", "-C", str(ROOT)]
FQBN = "m5stack:esp32:m5stack_atom:PartitionScheme=min_spiffs"
SOURCE_FILES = (
    "watchsync.ino", "time_policy.h", "transmission_policy.h", "time_zones.h",
    "web_ui.h", "wifi_config.h", "device_config.h", "wifi_picker.h",
    "ota_policy.h", "firmware_update.h", "ota_ui.h",
)


def run(command, **kwargs):
    return subprocess.check_output(command, text=True, **kwargs).strip()


def inside(path, directory):
    return path == directory or directory in path.parents


def source_commit():
    commit = run(GIT + ["rev-parse", "HEAD"])
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise SystemExit("Expected an exact source commit.")
    if run(GIT + ["status", "--porcelain", "--untracked-files=normal"]):
        raise SystemExit("Commit the reviewed source before creating a distribution package.")
    return commit


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", type=Path, required=True,
                        help="New empty build directory outside this repository.")
    parser.add_argument("--arduino-data", type=Path, required=True,
                        help="The installed Arduino data directory to map to /arduino.")
    parser.add_argument("--arduino-cli", default="arduino-cli")
    parser.add_argument("--config-file", type=Path)
    parser.add_argument("--map-root", type=Path, action="append", default=[],
                        help="Additional external toolchain root to anonymize.")
    parser.add_argument("--check-only", action="store_true",
                        help="Compile working source without producing a release package.")
    args = parser.parse_args()
    workspace = args.workspace.resolve()
    if inside(workspace, ROOT):
        raise SystemExit("Build output must remain outside the repository.")
    if workspace.exists() and any(workspace.iterdir()):
        raise SystemExit("Build workspace must be empty; existing files are never removed.")
    commit = None if args.check_only else source_commit()
    actual = {p.name for p in (ROOT / "watchsync").iterdir() if p.is_file()}
    if actual != set(SOURCE_FILES):
        raise SystemExit("Unexpected or missing sketch file; review the explicit staging list.")
    workspace.mkdir(parents=True, exist_ok=True)
    sketch = workspace / "watchsync"
    sketch.mkdir()
    for filename in SOURCE_FILES:
        shutil.copyfile(ROOT / "watchsync" / filename, sketch / filename)
    cli = [args.arduino_cli]
    if args.config_file:
        cli += ["--config-file", str(args.config_file.resolve())]
    cli_env = os.environ.copy()
    cli_env["ARDUINO_DIRECTORIES_DATA"] = str(args.arduino_data.resolve())
    cli_env["ARDUINO_DIRECTORIES_DOWNLOADS"] = str(workspace / "arduino-downloads")
    cli_env["ARDUINO_DIRECTORIES_USER"] = str(workspace / "arduino-user")
    version = run(cli + ["version"], env=cli_env)
    if not re.search(r"Version:\s*1\.5\.1\b", version):
        raise SystemExit("Distribution builds require Arduino CLI 1.5.1.")
    build = workspace / "build"
    base = cli + ["compile", "--fqbn", FQBN, "--build-path", str(build)]
    properties = dict(line.split("=", 1) for line in run(base + ["--show-properties=expanded", str(sketch)], env=cli_env).splitlines() if "=" in line)
    platform = Path(properties.get("runtime.platform.path", ""))
    if platform.name != "3.3.10" or not (platform / "platform.txt").is_file():
        raise SystemExit("Expected the installed M5Stack ESP32 3.3.10 platform.")
    recipes = (platform / "platform.txt").read_text(encoding="utf-8")
    for language in ("c", "cpp", "S"):
        recipe = re.search(rf"^recipe\.{language}\.o\.pattern=(.*)$", recipes, re.M)
        if not recipe or "{compiler." + language + ".extra_flags}" not in recipe[1]:
            raise SystemExit("The selected core does not apply prefix flags to every compiler recipe.")
    mappings = [(ROOT, "/source"), (workspace, "/build"), (args.arduino_data.resolve(), "/arduino")]
    mappings += [(p.resolve(), f"/toolchain/{index}") for index, p in enumerate(args.map_root)]
    flags = " ".join(f'"-ffile-prefix-map={old.as_posix()}={new}"' for old, new in mappings)
    command = base + ["--clean"]
    for language in ("c", "cpp", "S"):
        command += ["--build-property", f"compiler.{language}.extra_flags={flags}"]
    build_env = cli_env.copy()
    if commit:
        # GCC uses the reviewed commit's time for __DATE__/__TIME__ rather than
        # the operator's wall clock. This does not promise bit-for-bit builds.
        build_env["SOURCE_DATE_EPOCH"] = run(GIT + ["show", "-s", "--format=%ct", commit])
    subprocess.run(command + [str(sketch)], check=True, env=build_env)
    binary = build / "watchsync.ino.bin"
    # Keep manifests traceable to the reviewed sources without exposing host paths.
    source_hashes = {name: hashlib.sha256((sketch / name).read_bytes()).hexdigest() for name in SOURCE_FILES}
    (workspace / "source-hashes.json").write_text(json.dumps(source_hashes, indent=2) + "\n", encoding="utf-8")
    if args.check_only:
        from package_firmware import validate_image
        validate_image(binary, ROOT)
        print("PASS: clean Atom Lite compilation and distribution-image checks; no release package created.")
        return
    package = workspace / "package"
    subprocess.run([sys.executable, str(ROOT / "tools/package_firmware.py"), str(binary), str(package), "--commit", commit], check=True)
    subprocess.run([sys.executable, str(ROOT / "tools/check_public_tree.py"), "--artifacts", str(package), "--artifacts-only"], check=True)
    print(f"Release package: {package}")


if __name__ == "__main__":
    main()
