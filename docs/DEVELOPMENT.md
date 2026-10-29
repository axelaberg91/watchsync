# Development

Original documentation by **axelaberg91**, CC BY-NC 4.0.

## Supported build

Use Arduino CLI **1.5.1**, M5Stack ESP32 **3.3.10**, classic M5 Atom Lite,
antenna GPIO25 and **Minimal SPIFFS (1.9 MB APP with OTA/190 KB SPIFFS)**.
The single supported profile configures Wi-Fi through the AP portal and always
includes local OTA. No secrets header or compiled-credential profile is supported.

For interactive development, open the inner `watchsync/watchsync.ino` in Arduino
IDE. Keep the headers beside the sketch. The outer repository folder may have
any name. Follow the [website's installation guide](https://watchsync.axelaberg91.workers.dev/diy.html#firmware)
for first USB installation, including flash erasure. Compatible later OTA updates
retain saved settings. See [OTA.md](OTA.md) for implementation boundaries.

## Distribution builds

Commit the reviewed source before packaging. `tools/build_firmware.py` refuses a
dirty checkout, stages an explicit list of sketch files, checks the pinned board
package's C, C++ and assembly recipes, and applies GCC `-ffile-prefix-map` to the
source, build and Arduino data roots. It forces a clean compilation so a cached
core cannot reintroduce workstation paths. Unrecognized sketch files are refused.

The CI job installs dependencies in isolated runner directories and produces a
`watchsync-atom-lite-ota` artifact. To use an already installed local toolchain:

```sh
python tools/build_firmware.py --workspace /tmp/watchsync-build --arduino-data /path/to/arduino-data
```

The workspace must be empty and outside the repository. For additional external
toolchain roots, repeat `--map-root /path/to/toolchain`. Use `--config-file` when
the CLI has a separate configuration file. `--check-only` compiles working source
and runs image checks without creating a release package or claiming a source
commit. It is intended for local review before committing.

The package contains the versioned application, release notes, licensing notices, `manifest.json` and
`SHA256SUMS`. The manifest records the exact source commit, hardware profile,
application size and SHA-256. Package checks reject private user-directory paths,
wrong targets, damaged images and credential-bearing staging. The public-tree
scanner then checks the complete release directory for recognized sensitive data.

Attach the reviewed package, release notes and applicable licensing notices to
GitHub Releases. Distribution binaries and generated build output stay outside
Git. Keep older published release bytes and original historical archives
unchanged. A different application requires a new version.

## Automated checks

`.github/workflows/ci.yml` is the authoritative firmware check list. It runs C++
protocol, schedule, OTA-policy, OTA-server and device-configuration fixtures;
JavaScript UI/request checks; packaging failure-path checks; and the pinned
Atom compilation. The separate privacy workflow checks source and reachable Git
history. CI does not upload to a device or publish a release automatically.

The C++ fixtures need a C++17 compiler. For example:

```sh
g++ -std=c++17 -O2 tests/logic_test.cpp -o /tmp/watchsync-logic-test
/tmp/watchsync-logic-test
node tests/web_ui_test.js
python tests/package_firmware_test.py
```

A passing host fixture or compilation does not establish physical RF reception,
Wi-Fi provisioning, flash persistence, watchdog recovery, OTA power-loss behavior
or long-duration heap stability. Those require explicit device testing.

## Configuration and attribution

Radio settings use `watchsync`, Wi-Fi records use `watchsyncwifi`, and device
identity uses `watchsyncdev` in NVS. Historical namespace literals remain
read-only migration keys. Confirmed time trust is not restored from flash.

Preserve inherited authorship and license boundaries described in
[LICENSE.md](../LICENSE.md). Create changes on a separate branch and leave pull
requests open for review. User guides live on the website rather than being
duplicated here.

References: [Arduino CLI compile](https://docs.arduino.cc/arduino-cli/commands-reference/arduino-cli_compile)
and [GCC prefix mapping](https://gcc.gnu.org/onlinedocs/gcc/Overall-Options.html).

