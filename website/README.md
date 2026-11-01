# Watchsync website

Source for **https://axelaberg91.github.io/watchsync/** in the public
`axelaberg91/watchsync` repository. Original website, guides and assets by
axelaberg91: CC BY-NC 4.0. The repository's [licensing notice](../LICENSE.md)
defines its scope and preserves inherited attribution and third-party terms.

## Build and preview

From the repository root, using Python 3 and Node.js:

```sh
python website/tools/build_pages.py
python website/tools/check_pages.py
python website/tools/serve_pages.py
```

Open `http://127.0.0.1:8092/watchsync/`. `site-config.json` defines the public
address and project base path. The artifact is `website/dist/pages/`; it contains
only validated static website assets and `.nojekyll`. Source, local configuration,
release packages and development files are not served. Do not commit generated
pages or build output.

The language script derives the site root from its asset URL. The Pages builder
sets project base paths on the English and Swedish 404 pages. Production follows
PR review and merge to `master`, then the [Pages workflow](../.github/workflows/pages.yml).
See [site-deployment.md](site-deployment.md) for setup and verification.

## Downloads and contact

Firmware and printable downloads use versioned GitHub Release assets from the
same repository. Source and current CAD stay in Git; large files stay in Releases.
Keep existing release bytes immutable.

`publication.json` records the repository and expected release hashes. Deployment
verifies public release assets before uploading the Pages artifact. Use
`tools/add_release.py` with a checked local package, date and both language
summaries to register metadata and hashes. It does not upload assets or publish.

Contact and interest enquiries use Formspree. Recipient settings stay in its
dashboard; no personal support email is rendered. The public endpoint is not an
API credential. Never submit a real enquiry as a test.

## Device interface documentation

Guides describe the current released interface. Both languages retain English
device interface names. Update affected guides together when behavior changes.
A staged package does not make its download available to visitors.

## Languages

English HTML lives in `site/`; reviewed Swedish copy lives in
`translations/sv.json`. Validation generates `site/sv/` before checking both
versions. Never edit generated pages. New visible text requires a dictionary
entry; device UI names and code examples retain English spelling. Update release
summaries in both languages in `releases.json`.

Language links work without JavaScript. With JavaScript, the first visit uses
the browser's preferred language (`sv` / `sv-*` selects Swedish). A direct Swedish
URL selects Swedish. A valid `?lang=en` or `?lang=sv` choice takes precedence and
is saved locally when storage is available; stored choices override browser
preferences. Switching preserves the current page, query parameters and fragment.
No language data is sent to another service. Preferences saved on the old domain
cannot automatically transfer to the new origin.

## Legacy address

`src/legacy_redirect.js` preserves old printed QR codes and installed firmware
help links. It redirects GET and HEAD to the same Pages page, preserves query
parameters and maps previous extensionless routes to HTML URLs. Its destination
origin is fixed. Preserve the old address while older devices depend on it.

Maintain the redirect separately from Pages: validate the merged
`src/legacy_redirect.js` with `node website/tools/check_redirect.mjs`, then update
the existing Cloudflare Worker through its authenticated API or dashboard.
Preserve its hostname, fixed destination and disabled preview URLs. Keep platform
credentials outside the repository. The redirect has no automatic source-build
trigger, and no Cloudflare dependency is required to build the website.
