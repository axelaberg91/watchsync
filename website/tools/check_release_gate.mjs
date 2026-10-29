// SPDX-License-Identifier: CC-BY-NC-4.0
// Exercise missing, altered and mismatched public release assets without network access.
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { verifyReleaseAssets } from './verify_release_assets.mjs';

const hash = bytes => createHash('sha256').update(bytes).digest('hex');
const version = '1.12.2';
const filename = `watchsync-${version}.bin`;
const application = Buffer.from('credential-free application fixture');
const manifest = { version, file: filename, kind: 'application-only', containsCompiledWifiCredentials: false,
  core: 'M5Stack ESP32 3.3.10', commit: 'a'.repeat(40),
  target: 'M5 Atom Lite (classic ESP32), GPIO25', partitionScheme: 'min_spiffs', size: application.length, sha256: hash(application) };
const firmwareFiles = new Map([[filename, application], ['manifest.json', Buffer.from(JSON.stringify(manifest))],
  ['SHA256SUMS', Buffer.from(`${manifest.sha256}  ${filename}\n`)]]);
const hardwareFiles = new Map(['base', 'lid', 'bobbin'].map(part => [`watchsync-${part}.stl`, Buffer.from(`print fixture ${part}`)]));
const metadata = files => [...files].map(([name, bytes]) => ({ name, size: bytes.length, sha256: hash(bytes) }));
const config = { schemaVersion: 1, state: 'ready', repository: 'axelaberg91/watchsync', firmwareVersion: version,
  firmware: { version, tag: 'v' + version, assets: metadata(firmwareFiles) },
  hardware: { tag: 'hardware-v14', assets: metadata(hardwareFiles) } };
const catalog = { repository: config.repository, releases: [{ version, sha256: manifest.sha256, size: application.length }] };
function network({ missing = false, altered = false, draft = false, wrongURL = false } = {}) {
  return async value => {
    const url = new URL(value);
    const tag = url.pathname.includes('/hardware-v14') ? 'hardware-v14' : 'v' + version;
    const files = tag === 'hardware-v14' ? hardwareFiles : firmwareFiles;
    if (url.hostname === 'api.github.com') return { ok: !missing, json: async () => ({ tag_name: tag, draft, prerelease: false,
      assets: [...files].map(([name, bytes]) => ({ name, size: bytes.length,
        browser_download_url: `https://${wrongURL ? 'example.invalid' : 'github.com'}/${config.repository}/releases/download/${tag}/${name}` })) }) };
    const bytes = altered ? Buffer.from('altered') : files.get(url.pathname.split('/').pop());
    return { ok: true, arrayBuffer: async () => bytes };
  };
}
assert.equal((await verifyReleaseAssets(config, network(), async () => catalog)).length, 6);
for (const change of [{ missing: true }, { altered: true }, { draft: true }, { wrongURL: true }])
  await assert.rejects(verifyReleaseAssets(config, network(change), async () => catalog));
let requested = false;
await assert.rejects(verifyReleaseAssets({ ...config, state: 'staged' }, async () => { requested = true; }));
assert.equal(requested, false, 'Staged publication must fail before any network access');
await assert.rejects(verifyReleaseAssets({ ...config, legacy: {} }, network(), async () => catalog));
await assert.rejects(verifyReleaseAssets(config, network(), async () => ({ ...catalog, releases: [] })));
console.log('Publication gate rejects staged, missing, draft, altered and mismatched assets and catalog data.');
