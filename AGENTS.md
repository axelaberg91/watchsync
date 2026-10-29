# Watchsync contributor instructions

- Use English for code, comments, device interface text, logs and developer
  documentation. The public website intentionally supports English and Swedish:
  maintaining its Swedish translation is an explicit project exception to the
  general English-only rule. Keep device UI names and code examples in English
  in both website languages.
- Fetch current remote state, use a separate branch and open a pull request.
  Leave it open unless the user explicitly authorizes merging.
- Use the GitHub noreply identity for original commits. Never import private Git
  history, local configuration, personal contact details or workstation paths.
- Preserve inherited authorship and licenses. Follow the licensing scope and
  future-contribution rules below; do not apply one license to the whole tree.

## Repository and documentation

`axelaberg91/watchsync` is the single active source repository for firmware, the
public website and current hardware. Historical experiments and immutable legacy
releases remain archived. Do not recreate a separate public file mirror.

- Keep the root README short and direct visitors to the website. User guides
  belong on the website; build, development and OTA internals belong in `docs/`
  and website deployment instructions in `website/site-deployment.md`.
- Keep only current editable hardware in the active source tree. Distribute
  firmware binaries, printable files and complete packages through Releases.
- Use the relevant owning instructions when needed: `docs/DEVELOPMENT.md` for
  builds, `docs/OTA.md` for OTA behavior, `website/README.md` for website sources
  and translations, and `website/site-deployment.md` for deployment.

## Content that must never enter repositories or the website

- Do not store, commit or publish AI-internal temporary notes, prompts,
  conversation transcripts, scratch plans, handoff summaries, session state,
  one-off audit reports or migration work logs. This applies to source, docs,
  private and public branches, commit, issue and PR text, CI output, releases,
  downloadable packages and published build artifacts as well as the website.
- Keep any necessary temporary working material outside repository checkouts and
  served directories. `.gitignore`, a private branch, `docs/`, `.github/` or an
  in-repository archive is not an exception.
- Do not retain owner-specific local test adaptations: workstation paths, local
  account or network assumptions, assistant sandbox/runtime dependencies,
  hard-coded personal-data fixtures, one-off environment patches, fake responses
  in product code or bypasses that make local checks appear successful.
- Keep maintained tests reproducible with synthetic, non-personal fixtures and
  documented toolchain configuration. Keep owner-specific environment overrides
  outside the repository and all distribution packages.
- Keep AGENTS.md limited to durable project rules and developer documentation
  limited to maintained build, architecture and operating information. PRs and
  release metadata may report relevant validation and provenance concisely;
  never include raw assistant working notes or temporary scan reports.
- Inspect changed files, Git diffs and distribution contents for these boundaries
  before pushing or publishing. Do not merely hide unwanted content from the UI.

## Website presentation and content

- Treat the website as the primary product and support experience. The homepage
  should explain what Watchsync does, show the actual product, offer ready-built
  and DIY paths, explain the three setup steps, and provide owner resources.
- Preserve the current restrained visual style: a light neutral background,
  white surfaces, dark readable type, subtle green accents, generous spacing and
  simple navigation. Use real product photos and plain device-interface
  screenshots. Avoid decorative AI-style advertising graphics.
- Keep the navigation easy to scan, with grouped Guides and visible language
  controls. Check changed layouts on mobile and desktop, including menus,
  keyboard focus, forms, image proportions and horizontal overflow.
- Write concise, practical copy for visitors and owners. Keep repository
  migrations, branch names, CI details and internal development commentary out of
  customer-facing instructions. Retain useful source, release and licensing links.
- Preserve the hobby-project voice and the origin story about poor DCF77
  reception. Explain USB-C power, 2.4 GHz Wi-Fi and browser configuration without
  an app where relevant. State the limited range and placement directly on the
  unit; distinguish tested compatibility from expected compatibility.
- Keep the ready-built path as an interest enquiry unless a different sales flow
  is requested. Do not turn an approved advertisement price into a permanent
  website promise or invent stock, delivery times or compatibility claims.
- Keep contact and purchase enquiries in the existing forms. Do not expose a
  personal recipient address or submit real enquiries as a test.
- Describe the current published device interface. Update affected guides when
  behavior changes, and never advertise an unpublished image as downloadable.

## Website languages

- Update English source and reviewed Swedish translations together, including
  release summaries. Edit `website/translations/sv.json`; do not hand-edit or
  commit generated `website/site/sv/` pages.
- Preserve visible language selection, browser-language detection, saved manual
  preference and page/query/fragment preservation when switching languages.
- Validate both languages after changes. Keep product facts and available
  downloads consistent across them.

## Website publication

- Publish production only from reviewed, merged `master` through Cloudflare
  Workers Builds. Keep the existing Worker and website address.
- Do not manually upload feature branches or unmerged PRs to production. Keep
  preview builds disabled unless separately requested.
- Keep one production build trigger, rooted at `website`, with the existing
  `website/**` watch paths. Read the deployment document before changing this.
- Run website validation, language checks and a Worker packaging dry run before
  opening a deployment-related PR. Keep the release verification gate enabled.
- Keep credentials in the platform's secret store. Do not commit `.env`,
  `.dev.vars`, tokens, logs, generated pages, dependency trees or build outputs.
- After an authorized merge and deployment, verify the deployed commit and the
  affected live pages, language controls, forms and download links. Distinguish
  local validation, merged source and verified production in status reports.

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

## Release notes

- Check each version's claims against its source changes and actual package.
  Explain user-visible changes, fixes, relevant limitations and any required
  action in plain language. Do not copy commit logs, patch markers or migration
  chatter into customer-facing release notes.
- For a packaging-only release, state that device behavior is unchanged. Keep
  durable build provenance and hashes in release manifests; exclude temporary
  scanner reports and work logs from repository and publication contents.
- Keep the website summaries, repository changelog when present and GitHub
  Release description consistent. Verify links and retain accurate attribution.
- Correct explanatory release text separately from immutable package contents.
  If notes are embedded in a published ZIP, preserve that ZIP and explain the
  correction externally; changed package bytes require a new release.

## Licensing: current status and future contributions

- `LICENSE.md`, `LICENSES/` and file-specific notices define the current scope.
  Keep website licensing text and release notices consistent with them.
- Original contributions by axelaberg91 are licensed under CC BY-NC 4.0 unless
  explicitly marked otherwise. This remains the default for future original code,
  documentation, CAD, STL/3MF designs, illustrations and other original assets,
  and only for rights held by axelaberg91. Preserve credit to axelaberg91, the
  license link and any required notices or indications of changes.
- CC BY-NC 4.0 permits non-commercial sharing and adaptation under its terms.
  Commercial permission for these original contributions must be obtained
  separately from axelaberg91. The rights holder may sell their own work and grant
  separate commercial permissions for rights they control.
- Inherited firmware from `tanvach/clocksync` and `tarohs/nisejjy` is documented
  in `LICENSE.md` as having unresolved licensing. Preserve its authorship and
  that qualification. Do not claim CC BY-NC 4.0 covers those portions or that the
  complete firmware is cleared for redistribution or commercial distribution.
- Mixed files retain separate licensing scopes. Refactoring, formatting,
  rebranding, rebuilding or creating a new Git history does not change the rights
  in inherited code. Arduino, ESP32 and other dependencies retain their licenses.
- Before adding third-party code, models, fonts, images or other assets, verify
  and record their source, actual terms and required attribution in maintained
  notices. Public availability alone is not a reuse license. Unresolved material
  must not be presented as licensed or silently folded into the original grant.
- Change the original-contribution licensing policy only on explicit user
  instruction and within the rights they control. Do not retroactively rewrite
  published license grants, strip notices or relicense third-party material.
- Resolve inherited licensing only from documented rights-holder permission or
  other verified evidence covering the relevant inherited material. Record the
  scope in `LICENSE.md` and synchronize affected website and future release
  notices. Preserve original historical packages unchanged.

Run the existing firmware and website checks plus
`python tools/check_public_tree.py --history` before publication. Scan release
staging separately using `--artifacts <directory> --artifacts-only`.
