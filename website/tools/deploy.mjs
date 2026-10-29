// SPDX-License-Identifier: CC-BY-NC-4.0
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { readFile } from 'node:fs/promises';
import { verifyReleaseAssets } from './verify_release_assets.mjs';

// A guard against accidentally publishing a local checkout or PR branch.
// Branch permissions and the Cloudflare trigger remain the authorization controls.
if (process.env.WORKERS_CI !== '1' || process.env.WORKERS_CI_BRANCH !== 'master') {
  console.error('Production deployment requires Cloudflare Workers Builds on master. Merge the reviewed pull request first.');
  process.exit(1);
}

try {
  const config = JSON.parse(await readFile(new URL('../migration.json', import.meta.url), 'utf8'));
  const checked = await verifyReleaseAssets(config);
  console.log(`Verified ${checked.length} release assets before production deployment.`);
} catch (error) {
  console.error(error.message);
  process.exit(1);
}

const website = fileURLToPath(new URL('../', import.meta.url));
const wrangler = fileURLToPath(new URL('../node_modules/wrangler/bin/wrangler.js', import.meta.url));
const result = spawnSync(process.execPath, [wrangler, 'deploy'], {
  cwd: website,
  env: process.env,
  stdio: 'inherit',
});
if (result.error) console.error(result.error.message);
process.exit(result.status ?? 1);
