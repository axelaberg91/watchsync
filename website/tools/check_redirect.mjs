// SPDX-License-Identifier: CC-BY-NC-4.0
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

const code = await readFile(new URL('../src/legacy_redirect.js', import.meta.url), 'utf8');
const worker = (await import('data:text/javascript;base64,' + Buffer.from(code).toString('base64'))).default;
for (const [path, expected] of [
  ['/', '/watchsync/'],
  ['/sv/', '/watchsync/sv/'],
  ['/manual.html?lang=sv&source=qr', '/watchsync/manual.html?lang=sv&source=qr'],
  ['/sv/manual', '/watchsync/sv/manual.html'],
  ['/watchsync/sv/manual.html', '/watchsync/sv/manual.html'],
  ['//untrusted.example/a', '/watchsync/untrusted.example/a.html'],
  ['/../manual.html', '/watchsync/manual.html'],
  ['/%2e%2e/manual.html', '/watchsync/manual.html'],
]) {
  for (const method of ['GET', 'HEAD']) {
    const response = await worker.fetch(new Request('https://legacy.example' + path, { method }));
    assert.equal(response.status, 308);
    assert.equal(response.headers.get('location'), 'https://axelaberg91.github.io' + expected);
  }
}
const rejected = await worker.fetch(new Request('https://legacy.example/', { method: 'POST' }));
assert.equal(rejected.status, 405);
assert.equal(rejected.headers.get('location'), null);
console.log('Legacy redirects preserve page paths and queries within the fixed Pages origin.');
