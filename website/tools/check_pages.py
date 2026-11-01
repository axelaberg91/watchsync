#!/usr/bin/env python3
# SPDX-License-Identifier: CC-BY-NC-4.0
"""Check deployed-path navigation and publication scope in the Pages artifact."""
from html.parser import HTMLParser
import json
from pathlib import Path
from urllib.parse import unquote, urljoin, urlsplit

ROOT = Path(__file__).resolve().parents[1]
ARTIFACT = ROOT / 'dist' / 'pages'
CONFIG = json.loads((ROOT / 'site-config.json').read_text(encoding='utf-8'))
BASE = CONFIG['basePath']
ORIGIN = 'https://watchsync.example'


class Page(HTMLParser):
    def __init__(self, url):
        super().__init__()
        self.url, self.links, self.ids = url, [], set()

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == 'base':
            self.url = urljoin(self.url, attrs['href'])
        if 'id' in attrs:
            self.ids.add(attrs['id'])
        for key in ('href', 'src', 'action'):
            if key in attrs and tag != 'base':
                self.links.append(attrs[key])


def check():
    pages = {}
    for path in sorted(ARTIFACT.rglob('*')):
        assert not path.is_symlink(), 'Publication must not contain symbolic links'
        if not path.is_file():
            continue
        assert path.suffix in ('.html', '.css', '.js', '.svg', '.json', '.txt', '.png', '.webp') or path.name == '.nojekyll'
        if path.suffix == '.html':
            relative = path.relative_to(ARTIFACT).as_posix()
            page = Page(ORIGIN + BASE + relative)
            page.feed(path.read_text(encoding='utf-8'))
            pages[relative] = page
    assert 'index.html' in pages and 'sv/index.html' in pages and '404.html' in pages
    checked = 0
    for relative, page in pages.items():
        for link in page.links:
            target = urlsplit(urljoin(page.url, link))
            if target.netloc != urlsplit(ORIGIN).netloc:
                continue
            assert target.path.startswith(BASE), f'Link leaves project path: {relative}: {link}'
            target_name = unquote(target.path[len(BASE):])
            if target_name.endswith('/') or not target_name:
                target_name += 'index.html'
            target_file = (ARTIFACT / target_name).resolve()
            assert target_file.is_relative_to(ARTIFACT.resolve()), 'Link escapes the artifact'
            assert target_file.is_file(), f'Broken deployed link: {relative}: {link}'
            if target.fragment and target_name in pages:
                assert target.fragment in pages[target_name].ids, f'Missing deployed anchor: {relative}: {link}'
            checked += 1
    assert pages['404.html'].url == ORIGIN + BASE
    assert pages['sv/404.html'].url == ORIGIN + BASE + 'sv/'
    print(f'Checked {len(pages)} static pages and {checked} deployed-path links and anchors.')


if __name__ == '__main__':
    check()
