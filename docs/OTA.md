# Local firmware updates

Original implementation and documentation by **axelaberg91**, CC BY-NC 4.0.
Inherited firmware and external dependencies retain their separate status and terms.

## Supported image

The local update page accepts an **application-only** `watchsync-<VERSION>.bin`
for classic M5 Atom Lite, ESP32, antenna GPIO25. Use the M5Stack ESP32 3.3.10
Minimal SPIFFS profile with two application slots of 1,966,080 bytes each.
An application-only image cannot install or change the partition table.

Firmware is distributed through GitHub Releases with a manifest, checksum and
release notes. The [website's update guide](https://watchsync.axelaberg91.workers.dev/updates.html)
is the user-facing installation guide. Initial USB installation and recovery
instructions are in the [DIY guide](https://watchsync.axelaberg91.workers.dev/diy.html#firmware).

The page is available at `/update` and the standalone Firmware update section
through the trusted normal Wi-Fi network. It is unavailable through the setup
AP or captive-portal browser. Finish network scans and apply pending next-startup
network settings before starting an update.

OTA replaces the application and retains compatible saved Wi-Fi, hostname and
device settings. A temporary one-hour pass ends on restart. The radio requires
a fresh confirmed internet-time update before transmission resumes.

## Request and image checks

- Page token, local hostname/IP, same origin, LAN state, available memory and
  exact image size are checked before issuing a short-lived, single-use ticket.
- The streamed transfer rechecks the ticket and state, pauses radio output
  through the acknowledged pause path, and enforces a two-minute deadline.
- The image must match the ESP32 header, OTA target marker and embedded hash.
  The target marker and checksums are integrity/compatibility controls, not
  digital signatures or user authentication.
- The core validates the application before selecting the new boot slot.

The current local update page does not require an update password. Anyone with
access to the trusted local network may install a compatible image. HTTP does
not encrypt the transfer. Do not forward the device's web port to the internet.
The firmware does not download updates automatically or contact a cloud update
service.

## Confirmation and recovery

After an OTA reboot, immediate core auto-confirmation is deferred. Confirmation
requires at least 10 seconds, repeated loop progress, a live radio task,
initialized time-zone handling and registered update routes. Wi-Fi and internet
time are deliberately not confirmation requirements. If the checks do not pass
within 60 seconds, rollback is requested when a previous image is available.

The previous application slot remains intact during an interrupted upload.
Bootloader behavior after power loss or early startup depends on the ESP32 OTA
state machine. Physical interrupted-power recovery and rollback remain unverified
for this release. A first USB installation may have no previous image available;
USB remains the recovery path.

Older published packages remain immutable. Private historical source and recovery
snapshots are retained in the project archive; they are not supported alternative
build profiles in the consolidated repository. Do not reset shared branches or
replace an existing release to perform a downgrade.

## Packaging

Use [the distribution build](DEVELOPMENT.md#distribution-builds). The package
validator checks the classic ESP32 application header, embedded hash, Atom Lite
GPIO25 target, portal-only profile, source version, board/partition options,
credential-free staging and absence of private user paths. Generated output
stays outside Git and is scanned again before release upload.

References: [Espressif web OTA](https://docs.espressif.com/projects/arduino-esp32/en/latest/ota_web_update.html)
and [ESP-IDF OTA state machine](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/api-reference/system/ota.html).
