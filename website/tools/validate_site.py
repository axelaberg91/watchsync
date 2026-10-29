#!/usr/bin/env python3
# SPDX-License-Identifier: CC-BY-NC-4.0; original tooling by axelaberg91.
"""Check local navigation, public asset scope and release metadata."""
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import urlsplit, unquote
import json
import re
from build_translations import generate

SITE = Path(__file__).resolve().parents[1] / 'site'
PUBLICATION = json.loads((SITE.parent / 'migration.json').read_text(encoding='utf-8'))

def release_url(value, version, binary):
    url = urlsplit(value)
    base = '/' + PUBLICATION['repository'] + '/releases/'
    path = base + (f'download/v{version}/watchsync-{version}.bin' if binary else f'tag/v{version}')
    assert url.scheme == 'https' and url.netloc == 'github.com' and not url.query and not url.fragment
    assert url.path == path, 'Release URL must name the configured repository and exact version'

class Links(HTMLParser):
    def __init__(self):
        super().__init__()
        self.links, self.ids = [], set()
    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == 'base':
            assert attrs.get('href') in ('/', '/sv/'), 'Unexpected base URL'
            return
        if 'id' in attrs:
            assert attrs['id'] not in self.ids, 'Duplicate element ID'
            self.ids.add(attrs['id'])
        for key in ('href', 'src', 'action'):
            if attrs.get(key): self.links.append(attrs[key])

def validate():
    generate()
    forbidden = {'.ino','.h','.hpp','.cpp','.c','.bin','.elf','.map','.scad','.stl','.3mf'}
    # Audit the served tree; build dependencies are never embedded.
    for path in SITE.rglob('*'):
        if path.is_file() and 'downloads' not in path.relative_to(SITE).parts:
            assert path.suffix.lower() not in forbidden, f'Source or binary file in served assets: {path}'
    pages = {}
    for path in SITE.rglob('*'):
        if not path.is_file(): continue
        allowed_downloads={'watchsync-base.stl','watchsync-lid.stl','watchsync-bobbin.stl'}
        if path.parent == SITE/'downloads':
            assert path.name in allowed_downloads, f'Unexpected download: {path}'
            continue
        assert path.suffix in ('.html','.css','.js','.svg','.json','.txt','.png','.webp') or path.name in ('.nojekyll','_headers'), f'Unexpected public file: {path}'
        if path.suffix in ('.html','.css','.js','.svg','.json','.txt'):
            text = path.read_text(encoding='utf-8')
            assert not re.search(r'[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}', text), f'Email address in served content: {path}'
            assert not re.search(r'(?i)[A-Z]:[\\/]+Users[\\/]|/Users/[^/]+/', text), f'Local user path in served content: {path}'
            assert 'Watchsync-Public' not in text, f'Legacy repository link in served content: {path}'
        if path.suffix in ('.html','.json'):
            for version in re.findall(r'\b1\.\d+\.\d+\b', path.read_text(encoding='utf-8')):
                assert tuple(map(int,version.split('.'))) >= (1,12,1), f'Obsolete firmware reference: {path}: {version}'
        if path.suffix == '.html':
            parser = Links()
            parser.feed(path.read_text(encoding='utf-8'))
            pages[path.resolve()] = parser
    for path, parser in pages.items():
        for link in parser.links:
            u = urlsplit(link)
            if u.scheme or u.netloc: continue
            target = (path.parent / unquote(u.path)).resolve() if u.path else path
            assert target.is_relative_to(SITE.resolve()), f'Link leaves site: {link}'
            assert target.exists(), f'Broken link in {path.name}: {link}'
            if u.fragment and target in pages:
                assert u.fragment in pages[target].ids, f'Missing anchor in {path.name}: {link}'
    data = json.loads((SITE/'releases.json').read_text(encoding='utf-8'))
    assert data['schemaVersion'] == 1 and isinstance(data['releases'],list)
    assert re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', PUBLICATION['repository'])
    assert data['repository'] == PUBLICATION['repository'], 'Release repository does not match publishing target'
    versions = set()
    for r in data['releases']:
        assert isinstance(r['summarySv'], str) and r['summarySv'].strip(), 'Swedish release summary required'
        assert re.fullmatch(r'\d+\.\d+\.\d+',r['version']) and r['version'] not in versions
        assert tuple(map(int,r['version'].split('.'))) >= (1,12,1), 'Pre-release firmware must not appear on the website'
        versions.add(r['version'])
        assert re.fullmatch(r'[a-f0-9]{64}',r['sha256'])
        assert r['target'] == 'M5 Atom Lite (classic ESP32), GPIO25'
        assert r['partitionScheme'] == 'min_spiffs'
        assert type(r['size']) is int and 4096 <= r['size'] <= 1966080
        assert re.fullmatch(r'\d{4}-\d{2}-\d{2}',r['date'])
        release_url(r['binaryUrl'], r['version'], True)
        release_url(r['notesUrl'], r['version'], False)
    print(f'Validated {len(pages)} pages, local links, asset scope and {len(versions)} releases.')

if __name__ == '__main__': validate()
