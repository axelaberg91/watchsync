# Firmware changelog

Original release summaries by axelaberg91, CC BY-NC 4.0.
Inherited firmware and dependencies retain their separate licensing status.

Current downloads and installation instructions are on the
[Watchsync website](https://watchsync.axelaberg91.workers.dev/updates.html).

## 1.12.2

- Improve build and packaging checks for distributed firmware. Build paths use
  neutral prefixes, and packages are scanned for recognized private paths.
- No functional source changes from 1.12.1. Wi-Fi setup, transmission schedules,
  saved settings and the local interface retain the same behavior.

See the [1.12.2 release notes](releases/1.12.2.md) for installation requirements
and verification status.

## 1.12.1

- Use Wi-Fi setup on the device for every supported build. Remove support for
  compiled Wi-Fi credentials and keep local firmware updates always enabled.
- Clarify the first USB installation instructions: erase flash before uploading,
  then configure Wi-Fi through the setup network. Later OTA updates retain
  compatible saved settings.

## 1.12.0

- Improve initial Wi-Fi setup by preparing the network list before setup opens
  and keeping setup available when a connection attempt fails.
- Remove the firmware-update password. Updates use the device's normal local
  network; keep the device on a trusted network.
- Add Copy diagnostics and links to guides and support under Help.
- Show configured NTP servers in Device details.
- Update local Help for overnight automatic watch reception.

## 1.11.0

- Group hostname, Wi-Fi controls, IP address and the device web address under
  Network settings. Hostname and Wi-Fi have separate save controls.
- Rename Advanced settings to Radio/time settings.
- Move Firmware update into its own section.
- Place Factory reset below Help; typing `RESET` is still required.
- Rename the initial setup page to Network settings.

## Updating and verification

Use a compatible application image through the device's `/update` page.
The installed firmware determines the upload requirements: 1.11.0 and earlier
require the existing update password, normally `watchsync`; 1.12.0 and later do
not require an update password. Keep USB power connected until restart and
confirm the running version afterward. Normal OTA updates retain compatible
saved settings. A new confirmed internet time update is required before
transmission can resume.

Automated tests and successful builds do not establish physical OTA recovery,
watch reception or long-term device stability. Historical release packages
remain unchanged.
