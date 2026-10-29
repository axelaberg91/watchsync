// SPDX-License-Identifier: CC-BY-NC-4.0
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const deploy = fileURLToPath(new URL('./deploy.mjs', import.meta.url));
for (const [ci, branch] of [['', 'master'], ['1', 'feature/guard-check'], ['1', '']]) {
  const result = spawnSync(process.execPath, [deploy], {
    env: { ...process.env, WORKERS_CI: ci, WORKERS_CI_BRANCH: branch },
    encoding: 'utf8',
  });
  assert.equal(result.status, 1, 'Unsafe context must be rejected');
  assert.match(result.stderr, /Production deployment requires Cloudflare Workers Builds on master/);
}
console.log('Deployment guard rejects local, feature-branch and missing-branch contexts.');
