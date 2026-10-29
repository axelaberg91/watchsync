# Watchsync website

Website source for **https://watchsync.axelaberg91.workers.dev/** in the public
`axelaberg91/watchsync` repository.
Original website, guides and assets by axelaberg91: CC BY-NC 4.0.
The repository's [licensing notice](../LICENSE.md) defines its scope and preserves
inherited attribution and third-party terms.

Only `site/` is embedded in the deployed Worker. Firmware, local configuration
and development files are outside that serving directory. Contact is through the
Formspree form; no personal support email is rendered. The form's public endpoint
is not an API credential. Recipient settings remain in the Formspree dashboard.

Preview with `python -m http.server 8092 --directory website/site` from the
repository root. Validate with `python website/tools/validate_site.py`.

Production publishing follows PR review and merge to `master`, then Cloudflare
Workers Builds. Do not publish unmerged content manually. See
[site-deployment.md](site-deployment.md) for connection and verification steps.

From `website/`, run `npm ci`, `npm run build`, and `npm run check:package`.
`npm run deploy` is reserved for Cloudflare production builds on `master`.
No Cloudflare token is stored in this tree.

Firmware and printable downloads use versioned GitHub Release assets from the
same repository. Source code and current CAD stay in Git; large downloadable
files stay in Releases. Keep existing release bytes immutable.

`migration.json` records the public repository and expected release asset hashes.
Production deployment verifies the release assets before publishing; preserve this
check when updating release metadata.

## Device interface documentation

The manual, quick start, DIY, troubleshooting and update guides describe the
current released interface. Both website languages keep device interface names
in English. Update the guides together when that interface changes. A staged
package does not make its download available to visitors.

Use `tools/add_release.py` with a checked local release package, date and both
language summaries to register its metadata and asset hashes. The tool stages
files; it does not upload assets or publish the website.

## Languages

English HTML lives in `site/`; reviewed Swedish copy lives in
`translations/sv.json`. Validation generates `site/sv/` before checking both
versions. Never edit generated pages. New visible text requires a dictionary
entry; device UI names and code examples intentionally retain their original
English spelling. Update release summaries in both languages in `releases.json`.

The language links work without JavaScript. With JavaScript, the first visit
uses the browser's preferred language (`sv` / `sv-*` selects Swedish; all others
select English). A direct Swedish URL selects Swedish. A valid `?lang=en` or
`?lang=sv` choice takes precedence and is saved locally when storage is available;
stored choices override browser preferences. Switching preserves the current page,
query parameters and fragment. No language data is sent to a separate service.
