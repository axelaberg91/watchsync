# Watchsync contributor instructions

- Use English in source code, comments, logs and documentation. Maintain the
  explicitly requested Swedish website translation alongside English pages.
- Fetch current remote state, use a separate branch and open a pull request.
  Leave it open unless the user explicitly authorizes merging.
- Use the GitHub noreply identity for original commits. Never import private Git
  history, local configuration, personal contact details or workstation paths.
- Preserve inherited authorship and licenses. Original contributions by
  axelaberg91 default to CC BY-NC 4.0; inherited material is not relicensed.

## Repository and website

This is the single active source repository for firmware, the public website and
current hardware. Keep user guides on the website and developer instructions in
`docs/`. Historical experiments and immutable legacy releases remain archived.

- Publish production only from reviewed, merged `master` through Cloudflare
  Workers Builds. Keep the existing Worker and website address.
- Do not manually upload feature branches or unmerged PRs to production. Keep
  preview builds disabled unless separately requested.
- Run website validation, language checks and a Worker packaging dry run before
  opening a deployment-related PR. Enforce the migration gate before cutover.
- Keep credentials in the platform's secret store. Do not commit `.env`,
  `.dev.vars`, tokens, logs, generated pages, dependency trees or build outputs.
- Describe only the current published device interface in both website
  languages. Never advertise an unavailable firmware image as downloadable.

## Firmware and releases

- Support only the AP-configured classic Atom Lite build with OTA always present,
  antenna GPIO25, M5Stack ESP32 3.3.10 and Minimal SPIFFS. Preserve the compatible
  `WATCHSYNC-OTA:ATOM-LITE:GPIO25:v1` marker and saved-settings behavior.
- Never introduce a secrets-header build path or compile Wi-Fi credentials.
  Stage distribution builds from an explicit allowlist and neutral build paths.
- Keep firmware binaries, STL/3MF files and ZIP packages in versioned Releases,
  outside the Git source tree. Include exact source commit, target, toolchain,
  application size, checksums, release notes and verification status.
- Commit the source before packaging. Scan final binary/archive bytes for private
  data, then validate their checksums. Use a new version for changed bytes;
  never overwrite or silently rebuild an existing published release.
- Preserve the original historical packages unchanged in the archives. No old
  private source commit or restore reference belongs in a new public manifest.
- Erase flash for the first USB installation; later OTA updates retain compatible
  settings. Do not change device behavior merely to reorganize the repository.
- Host tests and successful builds do not verify physical OTA recovery, radio
  reception, enclosure fit or long-term operation. Do not flash a physical device
  without the user's authorization for that step.

Run the existing firmware and website checks plus
`python tools/check_public_tree.py --history` before publication. Scan release
staging separately using `--artifacts <directory> --artifacts-only`.
