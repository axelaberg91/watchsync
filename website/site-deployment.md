# Website deployment

Production: https://watchsync.axelaberg91.workers.dev/
Worker: `watchsync`. Repository: `axelaberg91/watchsync`.

## Publishing policy

Work on a separate branch, pass checks, and open a pull request. Publish only
after review and merge to `master`, through Cloudflare Workers Builds. Never
upload an unmerged branch manually. The deploy script rejects local runs and
branches other than `master`; this is an accident guard, not a replacement for
repository permissions and correct trigger configuration.

## One-time connection

At the approved cutover, connect the existing `watchsync` Worker to the new clean
repository's identity. A reused repository name does not transfer the previous
Cloudflare connection. Keep the current production connection until the new
repository and release packages pass review. Do not create another Worker or
leave two production build triggers active.

| Setting | Value |
| --- | --- |
| Repository | `axelaberg91/watchsync` |
| Production branch | `master` |
| Root directory | `website` |
| Build command | `npm run build` |
| Deploy command | `npm run deploy` |
| Preview / non-production branch builds | Disabled |
| Include build watch paths | `website/**` |
| Build variable | `NODE_VERSION=24.19.0` |
| Build variable | `PYTHON_VERSION=3.12` |

Workers Builds installs dependencies from `package-lock.json`. Wrangler is pinned
in `package.json`. Use the Cloudflare-managed build token; never commit a token
or add one to GitHub. Do not override injected `WORKERS_CI` or
`WORKERS_CI_BRANCH` variables.

## Checks and first build

GitHub validates the website and dry-runs Worker packaging on pull requests.
From `website/`, run `npm ci`, `npm run build` and `npm run check:package`
(Python 3 and Node.js required).

The staged tree is intentionally blocked from production. Register the clean
firmware package with `tools/add_release.py`, publish its exact assets and the
`hardware-v14` assets under the final public repository, then review a change to
`migration.json` setting `state` to `ready` and removing the temporary `legacy`
section. Run `node tools/verify_release_assets.mjs` from `website/` to verify
public readability, stable release state, exact filenames, sizes and SHA-256
checksums. `npm run deploy` repeats this check after checking its build context.
An incomplete or changed release blocks deployment before Wrangler starts.

After connecting, run the first build against merged `master`. Confirm the build
commit matches `master`, deployment succeeds, and the live homepage, DIY,
downloads and enquiry form load correctly. A configuration save or dry run alone
does not prove automatic deployment works. An open PR must leave production
unchanged. Do not submit an enquiry as a deployment smoke test.

For rollback, revert the website change in a PR and merge it. In an urgent
incident, restore a known-good production version in Cloudflare, then reconcile
`master` before the next automatic build.

## Serving boundaries

Only `site/` assets are embedded in the Worker. The builder rejects growth above
750 kB. Firmware, secrets and development files are outside the served tree.
Large downloads use versioned release URLs in the same public repository. Logs and
traces are enabled; version preview URLs are disabled.

Contact and purchase enquiries use Formspree. Recipient settings remain in its
dashboard; no personal support email is rendered. Historical public Git commits
are not rewritten by deployment changes.

References: [Build configuration](https://developers.cloudflare.com/workers/ci-cd/builds/configuration/)
and [Git integration](https://developers.cloudflare.com/workers/ci-cd/builds/git-integration/).

The two language versions share images and scripts. The bounded asset ceiling
is 750 kB to accommodate translated HTML without duplicating media.
