// SPDX-License-Identifier: CC-BY-NC-4.0
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import vm from 'node:vm';
import worker from '../dist/worker.mjs';

const script = await readFile(new URL('../site/assets/language.js', import.meta.url), 'utf8');
function visit({ path = '/manual.html#controls', current = 'en', browser = 'en-US', saved, blocked = false }) {
  const url = new URL(path, 'https://watchsync.example');
  const links = ['en', 'sv'].map(language => ({
    href: new URL((language === 'sv' ? '/sv/' : '/') + 'manual.html', url).href,
    dataset: { language },
  }));
  const navigation = { href: new URL((current === 'sv' ? '/sv/' : '/') + 'diy.html#coil', url).href, dataset: {} };
  let redirect;
  let ready;
  const storage = {
    getItem() { if (blocked) throw new Error('Disabled'); return saved; },
    setItem(key, value) { if (blocked) throw new Error('Disabled'); saved = value; },
  };
  vm.runInNewContext(script, {
    URL,
    navigator: { languages: [browser], language: browser },
    localStorage: storage,
    location: { href: url.href, replace(value) { redirect = value; } },
    document: {
      documentElement: { lang: current, dataset: { page: 'manual.html' } },
      addEventListener(event, callback) { ready = callback; },
      querySelectorAll(selector) { return selector === '[data-language]' ? links : [...links, navigation]; },
    },
  });
  ready?.();
  return { redirect: redirect ? new URL(redirect) : undefined, saved, links, navigation };
}

assert.equal(visit({ browser: 'sv-SE' }).redirect.pathname, '/sv/manual.html');
assert.equal(visit({ browser: 'sv-FI' }).redirect.hash, '#controls');
assert.equal(visit({ browser: 'SV' }).redirect.pathname, '/sv/manual.html');
for (const browser of ['en-GB', 'de-DE', 'fr', '']) assert.equal(visit({ browser }).redirect, undefined);
assert.equal(visit({ browser: 'sv-SE', saved: 'en' }).redirect, undefined);
assert.equal(visit({ browser: 'en-US', saved: 'sv' }).redirect.pathname, '/sv/manual.html');
assert.equal(visit({ current: 'sv', path: '/sv/manual.html', browser: 'en-US' }).redirect, undefined);
assert.equal(visit({ browser: 'sv', saved: 'unknown' }).redirect.pathname, '/sv/manual.html');
assert.equal(visit({ browser: 'sv', blocked: true }).redirect.pathname, '/sv/manual.html');
const explicit = visit({ path: '/manual.html?source=qr&lang=en#controls', browser: 'sv', saved: 'sv' });
assert.equal(explicit.redirect, undefined);
assert.equal(explicit.saved, 'en');
const switchURL = new URL(explicit.links[1].href);
assert.equal(switchURL.searchParams.get('source'), 'qr');
assert.equal(switchURL.searchParams.get('lang'), 'sv');
assert.equal(switchURL.hash, '#controls');
const blocked = visit({ path: '/manual.html?lang=en', browser: 'sv', blocked: true });
assert.equal(blocked.redirect, undefined);
assert.equal(new URL(blocked.navigation.href).searchParams.get('lang'), 'en');

for (const [path, status, language] of [['/', 200, 'en'], ['/sv/', 200, 'sv'], ['/sv/manual', 200, 'sv'], ['/missing', 404, 'en'], ['/sv/missing', 404, 'sv']]) {
  const response = await worker.fetch(new Request('https://watchsync.example' + path));
  assert.equal(response.status, status);
  assert.match(await response.text(), new RegExp('<html lang="' + language + '"'));
}
const head = await worker.fetch(new Request('https://watchsync.example/sv/', { method: 'HEAD' }));
assert.equal(await head.text(), '');
console.log('Language selection, saved choices, blocked storage, links and localized Worker routes passed.');
