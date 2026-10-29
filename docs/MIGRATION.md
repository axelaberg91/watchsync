# Repository migration

This document describes a reviewed transition to one active public repository,
`axelaberg91/watchsync`, while retaining the existing Cloudflare Worker and URL.
The preparation repository is private until the final publication step.

## Review checkpoint

Review the preparation pull request and its checks before merging. Preparing this
source does not authorize a merge, public visibility change or production deploy.
Keep the existing repositories and live website operational during review.

The new repository starts from a clean, reviewed source snapshot. Do not merge,
fetch or push historical private branches into it. Use fresh clones with the
GitHub noreply commit identity, including for migration commits and tags.

## Release preparation

1. Commit the complete source and run firmware, website and privacy checks.
2. Build the privacy-only firmware 1.12.2 package from that exact clean commit.
   Confirm the supported hardware profile and scan the final application bytes.
   Retain manifest, SHA256SUMS and release notes. Do not copy legacy binaries with
   personal build paths into the new public release.
3. Prepare the `hardware-v14` print release from the current three STL parts.
   Include the source revision, checksums, license and a clear statement that
   physical print verification is outstanding. Scan the final archive contents.
4. Keep release packages outside Git. GitHub Actions artifacts are review outputs;
   producing an artifact does not publish a GitHub Release or prove hardware tests.

Never replace bytes under an existing version. Original historical packages stay
unchanged in the archives, even when new public builds use neutral paths.

## Cutover after explicit review and merge authorization

1. Confirm no upstream work has arrived since the snapshot. Preserve any new work
   separately and incorporate reviewed source changes without importing history.
2. Verify recoverable copies of both existing repositories, tags, published release
   assets, metadata and active development branches. Keep archives private.
3. Rename the existing private `watchsync` to `watchsync-private-archive`. Rename
   the reviewed clean repository to `watchsync`. Names do not transfer repository
   IDs, permissions, integration settings or release assets.
4. Publish the clean firmware and `hardware-v14` release packages. Verify every
   asset's name, byte size and checksum at its final URL. Register the verified
   firmware package with the website release tool and update both languages.
5. Complete the website's machine-readable migration gate. Public visibility and
   final release checks must succeed before the production deployment is enabled.
6. Connect the existing Cloudflare Worker to the new repository ID. Keep production
   branch `master`, root `website`, watch path `website/**`, current build commands
   and previews disabled. Disable the old repository's production trigger before
   enabling the replacement; do not run two production sources.
7. Validate home, guides, forms (without submitting), both languages, firmware and
   STL downloads. Check the deployed commit matches the reviewed production commit.
8. Make the old `Watchsync-Public` repository private only after its required release
   metadata and assets are preserved and website links use the new repository.
   Rename it to `watchsync-public-archive` and archive both legacy repositories
   after confirming their integrations cannot deploy production.

Old direct GitHub links may stop working when the old public repository becomes
private. Printed QR codes retain the existing website address. Do not promise
redirect compatibility for old raw commit URLs. Already downloaded public copies
cannot be recalled by changing repository visibility.

## Recovery

Until the final smoke checks pass, retain the previous Worker deployment and the
old source-to-Worker connection details privately. A failed cutover must leave
the archives and release originals intact. Restore the last known-good Worker
deployment if needed, then reconcile the reviewed production branch before the
next build. Stop if permissions or repository identity do not match the plan.
