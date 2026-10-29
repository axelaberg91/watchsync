// SPDX-License-Identifier: CC-BY-NC-4.0
// Verify the public release files before deploying links to them.
import { createHash } from 'node:crypto';
import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';

const digest = bytes => createHash('sha256').update(bytes).digest('hex');
const requireValue = (condition, message) => { if (!condition) throw new Error(message); };

export async function verifyReleaseAssets(config, fetcher = fetch, catalogReader = async () =>
  JSON.parse(await readFile(new URL('../site/releases.json', import.meta.url), 'utf8'))) {
  requireValue(config.schemaVersion === 1 && config.state === 'ready' && !config.legacy,
    'Publication is staged. Complete release verification and remove legacy migration data before production deployment.');
  requireValue(/^[A-Za-z0-9_.-]+\/[A-Za-z0-9_.-]+$/.test(config.repository), 'Invalid publication repository');
  requireValue(config.firmware?.version === config.firmwareVersion && config.firmware.tag === 'v' + config.firmwareVersion,
    'Firmware package has not been registered');
  requireValue(config.hardware?.tag === 'hardware-v14', 'Expected hardware release is missing');
  const checked = [];
  for (const group of [config.firmware, config.hardware]) {
    requireValue(Array.isArray(group.assets) && group.assets.length > 0, 'Expected release assets are missing');
    const response = await fetcher(`https://api.github.com/repos/${config.repository}/releases/tags/${group.tag}`,
      { headers: { Accept: 'application/vnd.github+json' }, signal: AbortSignal.timeout(20000) });
    requireValue(response.ok, `Release ${group.tag} is not publicly readable`);
    const release = await response.json();
    requireValue(release.tag_name === group.tag && !release.draft && !release.prerelease && Array.isArray(release.assets),
      `Release ${group.tag} is not a published stable release`);
    const files = new Map();
    for (const expected of group.assets) {
      requireValue(/^[A-Za-z0-9_.-]+$/.test(expected.name) && /^[a-f0-9]{64}$/.test(expected.sha256) &&
        Number.isInteger(expected.size) && expected.size > 0 && expected.size <= 8000000, 'Invalid expected release asset');
      const asset = release.assets.find(item => item.name === expected.name);
      const url = `https://github.com/${config.repository}/releases/download/${group.tag}/${expected.name}`;
      requireValue(asset?.browser_download_url === url && asset.size === expected.size,
        `Release ${group.tag} is missing the expected ${expected.name}`);
      const download = await fetcher(url, { signal: AbortSignal.timeout(30000) });
      requireValue(download.ok, `Cannot download ${group.tag}/${expected.name}`);
      const bytes = Buffer.from(await download.arrayBuffer());
      requireValue(bytes.length === expected.size && digest(bytes) === expected.sha256,
        `Checksum mismatch for ${group.tag}/${expected.name}`);
      files.set(expected.name, bytes);
      checked.push(`${group.tag}/${expected.name}`);
    }
    if (group === config.firmware) {
      const filename = `watchsync-${config.firmwareVersion}.bin`;
      requireValue(files.has(filename) && files.has('manifest.json') && files.has('SHA256SUMS'), 'Firmware package is incomplete');
      const manifest = JSON.parse(files.get('manifest.json').toString('utf8'));
      requireValue(manifest.version === config.firmwareVersion && manifest.file === filename &&
        manifest.kind === 'application-only' && manifest.containsCompiledWifiCredentials === false &&
        manifest.target === 'M5 Atom Lite (classic ESP32), GPIO25' && manifest.partitionScheme === 'min_spiffs' &&
        manifest.core === 'M5Stack ESP32 3.3.10' && /^[a-f0-9]{40}$/.test(manifest.commit) &&
        manifest.size === files.get(filename).length && manifest.sha256 === digest(files.get(filename)),
        'Published firmware manifest does not match the expected application');
      requireValue(files.get('SHA256SUMS').toString('utf8').trim() === `${manifest.sha256}  ${filename}`,
        'Published firmware checksum list does not match its manifest');
    } else {
      for (const part of ['base', 'lid', 'bobbin']) requireValue(files.has(`watchsync-${part}.stl`), 'Printable hardware package is incomplete');
    }
  }
  const catalog = await catalogReader();
  const current = catalog.releases.find(item => item.version === config.firmwareVersion);
  const application = config.firmware.assets.find(item => item.name === `watchsync-${config.firmwareVersion}.bin`);
  requireValue(catalog.repository === config.repository && current?.sha256 === application.sha256 && current.size === application.size,
    'Website release catalog does not match the verified firmware');
  return checked;
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  try {
    const config = JSON.parse(await readFile(new URL('../migration.json', import.meta.url), 'utf8'));
    const assets = await verifyReleaseAssets(config);
    console.log(`Verified ${assets.length} public release assets and the website release catalog.`);
  } catch (error) {
    console.error(error.message);
    process.exitCode = 1;
  }
}
