# Watchsync

A radio time-signal transmitter for the classic **M5Stack Atom Lite (ESP32)**.

**[Website, setup and DIY guide](https://watchsync.axelaberg91.workers.dev/)**

The website is the main place for installation, downloads and support. This
repository contains the firmware, website, editable enclosure design and checks.
Versioned firmware and print packages are distributed through GitHub Releases.

| Directory | Contents |
| --- | --- |
| `watchsync/` | Arduino sketch and supporting headers |
| `website/` | Website source, translations and Cloudflare Worker |
| `hardware/` | Current editable enclosure design and verification scope |
| `tests/`, `tools/` | Build, test, packaging and publication checks |
| `docs/` | Development, release and deployment instructions |

To open the firmware in Arduino IDE, open `watchsync/watchsync.ino` inside the
downloaded repository. Keep its headers in the same sketch folder. The supported
profile is M5Stack ESP32 **3.3.10**, classic Atom Lite, **GPIO25** and Minimal SPIFFS
with OTA. Wi-Fi is configured on the device; credentials are never compiled in.

See the [firmware changelog](docs/CHANGELOG.md),
[development](docs/DEVELOPMENT.md), [firmware updates](docs/OTA.md) and
[website deployment](website/site-deployment.md).

Original contributions by **axelaberg91** use **CC BY-NC 4.0**. Inherited firmware
from [Tan Vachiramon](https://github.com/tanvach/clocksync) and
[SASAKI Taroh](https://github.com/tarohs/nisejjy) is not relicensed; its licensing
status remains unresolved. See [LICENSE.md](LICENSE.md).
