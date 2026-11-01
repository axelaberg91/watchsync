# Website deployment

Production: https://axelaberg91.github.io/watchsync/
Repository: `axelaberg91/watchsync`. Publishing branch: `master`.

## Publishing policy

Work on a separate branch, pass checks, and open a pull request. Only reviewed,
merged `master` may publish through `.github/workflows/pages.yml`. PRs run checks
without deploying. Manual workflow dispatch is also restricted to `master`.
Keep the Pages environment restricted to that branch.

Configure **Settings → Pages → Build and deployment → Source → GitHub Actions**.
The workflow uses `GITHUB_TOKEN` and GitHub's deployment identity; do not create
or commit a separate token. Enforce HTTPS.

## Build and deployment checks

The build job uses Python 3.12 and Node.js 24.19.0, generates both languages,
validates links and release metadata, and creates `website/dist/pages/`. Only
validated static assets and `.nojekyll` enter the artifact. Source, firmware,
CAD, dependencies and local files remain outside it.

Static-path checks resolve links and anchors beneath `/watchsync/`, including
404 base paths. PR checks also exercise language selection, saved preferences,
blocked storage, query and fragment preservation, release links and legacy
redirects. The Worker dry run remains a transition compatibility check.

Before artifact upload, `node tools/verify_release_assets.mjs` from `website/`
verifies public readability, release state, filenames, sizes and SHA-256 hashes.
Missing or changed assets block publication. Only the deploy job has
`pages: write` and `id-token: write`. Actions are pinned to reviewed commits.

Run locally from the repository root:

```sh
python website/tools/build_pages.py
python website/tools/check_pages.py
node website/tools/check_releases.mjs
node website/tools/check_release_gate.mjs
node website/tools/check_redirect.mjs
```

For transition Worker checks, run `npm ci`, `npm run build`,
`npm run check:package` and `node tools/check_languages.mjs` from `website/`.
Pages itself does not require Wrangler.

## Production verification

Confirm the Pages deployment identifies the intended merged commit. Check both
languages, guides, direct URLs, language controls, mobile and desktop navigation,
images, unknown-path 404 behavior and downloads. Inspect form endpoint, fields
and language values without submitting an enquiry. Check publication file scope.

GitHub Pages owns response headers, caching and error-page selection. The old
Worker's custom headers and localized error routing do not carry over. Missing
paths use the English root `404.html`; the Swedish 404 remains directly available.
Use `.html` URLs for canonical page links.

## Old-address compatibility

The old `watchsync` Cloudflare Worker keeps its hostname as a small redirect
using `src/legacy_redirect.js`. Leave the full site in place until Pages passes
live verification. Disable the former automatic full-site build trigger before
activating the redirect so later pushes cannot restore the old site over it.

The redirect preserves page/language paths and queries. Browsers retain a fragment
when the destination supplies no replacement. Old extensionless routes map to
`.html` URLs. GET and HEAD are accepted; form submissions remain direct to
Formspree. A fixed destination prevents requests from selecting another origin.
Preserve the address for existing printed parts and firmware.

If initial verification fails, leave the full-site Worker serving the site and
correct Pages before cutover. After cutover, restore a known-good Pages artifact
or revert through a PR. Never redirect Pages back to the legacy address while
the legacy address points to Pages.

GitHub references: [custom workflows](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages)
and [publishing configuration](https://docs.github.com/en/pages/getting-started-with-github-pages/configuring-a-publishing-source-for-your-github-pages-site).
