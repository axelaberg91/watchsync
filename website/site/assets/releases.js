// SPDX-License-Identifier: CC-BY-NC-4.0; original website code by axelaberg91.
// This public page reads published metadata only. It never contacts a device.
'use strict';
(async function () {
  const swedish = document.documentElement.lang === 'sv';
  const metadataURL = new URL('../releases.json', document.currentScript.src);
  const empty = document.querySelector('.release-empty');
  const list = document.getElementById('releaseList');
  function node(tag, text, className) {
    const el = document.createElement(tag);
    if (text) el.textContent = text;
    if (className) el.className = className;
    return el;
  }
  function releaseURL(value, version, binary, repository) {
    const url = new URL(value);
    const prefix = '/' + repository + '/releases/';
    const expected = prefix + (binary ? 'download/v' + version + '/watchsync-' + version + '.bin' : 'tag/v' + version);
    if (url.protocol !== 'https:' || url.hostname !== 'github.com' || url.port || url.username || url.password ||
        url.search || url.hash || url.pathname !== expected) {
      throw new Error('Invalid release URL');
    }
    return url.href;
  }
  try {
    const response = await fetch(metadataURL, {cache: 'no-cache'});
    if (!response.ok) throw new Error('Release list unavailable');
    const data = await response.json();
    if (data.schemaVersion !== 1 || !Array.isArray(data.releases) ||
        !/^[A-Za-z0-9_.-]+\/[A-Za-z0-9_.-]+$/.test(data.repository)) throw new Error('Invalid release list');
    const cards = data.releases.map(release => {
      if (!/^\d+\.\d+\.\d+$/.test(release.version) || !/^[a-f0-9]{64}$/.test(release.sha256) ||
          release.target !== 'M5 Atom Lite (classic ESP32), GPIO25' || release.partitionScheme !== 'min_spiffs' ||
          !Number.isInteger(release.size) || release.size < 4096 || release.size > 1966080 ||
          typeof release.summary !== 'string' || typeof release.summarySv !== 'string') throw new Error('Invalid release');
      const card = node('article', '', 'release-entry');
      card.append(node('h3', 'Version ' + release.version));
      const target = swedish ? release.target.replace('classic ESP32', 'klassisk ESP32') : release.target;
      card.append(node('p', release.date + ' · ' + target, 'release-meta'));
      card.append(node('p', swedish ? release.summarySv : release.summary));
      const actions = node('div', '', 'actions');
      for (const [label, url, primary] of [
        [swedish ? 'Ladda ned watchsync.bin' : 'Download watchsync.bin', release.binaryUrl, true],
        [swedish ? 'Versionsinformation' : 'Release notes', release.notesUrl, false]
      ]) {
        const a = node('a', label, primary ? 'button primary' : 'button');
        a.href = releaseURL(url, release.version, primary, data.repository);
        if (primary) a.textContent = (swedish ? 'Ladda ned ' : 'Download ') + new URL(a.href).pathname.split('/').pop();
        actions.append(a);
      }
      card.append(actions, node('p', 'SHA-256: ' + release.sha256, 'release-meta'));
      card.lastChild.style.overflowWrap = 'anywhere';
      return card;
    });
    list.replaceChildren(...cards);
    if (cards.length) empty.hidden = true;
  } catch (error) {
    empty.querySelector('h3').textContent = swedish ? 'Versionsinformationen är inte tillgänglig' : 'Release information unavailable';
    empty.querySelector('p').textContent = swedish
      ? 'Releaselistan kunde inte hämtas. På GitHub Releases finns nedladdningar och versionsinformation.'
      : 'The release list could not be loaded. Check GitHub Releases for available downloads and notes.';
  }
})();
