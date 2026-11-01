#!/usr/bin/env python3
# SPDX-License-Identifier: CC-BY-NC-4.0
"""Build the validated static website for its configured GitHub Pages path."""
import json
from pathlib import Path
import shutil
from urllib.parse import urlsplit

from validate_site import validate

ROOT = Path(__file__).resolve().parents[1]
SITE = ROOT / 'site'
OUTPUT = ROOT / 'dist' / 'pages'


def build():
    config = json.loads((ROOT / 'site-config.json').read_text(encoding='utf-8'))
    base = config['basePath']
    address = urlsplit(config['url'])
    assert address.scheme == 'https' and address.netloc and address.path == base
    assert base.startswith('/') and base.endswith('/') and '..' not in base.split('/')
    assert not address.query and not address.fragment
    validate()
    assert OUTPUT.resolve() == ROOT.resolve() / 'dist' / 'pages'
    if OUTPUT.exists():
        assert not OUTPUT.is_symlink(), 'Build output must not be a symbolic link'
        shutil.rmtree(OUTPUT)
    OUTPUT.mkdir(parents=True)
    count = 0
    for source in sorted(SITE.rglob('*')):
        assert not source.is_symlink(), 'Symbolic links are not publication inputs'
        if not source.is_file():
            continue
        relative = source.relative_to(SITE)
        assert 'downloads' not in relative.parts, 'Downloads belong in GitHub Releases'
        if source.name in ('.nojekyll', '_headers'):
            continue
        target = OUTPUT / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        if source.suffix == '.html':
            body = source.read_text(encoding='utf-8')
            body = body.replace('<base href="/">', f'<base href="{base}">')
            body = body.replace('<base href="/sv/">', f'<base href="{base}sv/">')
            target.write_text(body, encoding='utf-8', newline='\n')
        else:
            shutil.copyfile(source, target)
        count += 1
    (OUTPUT / '.nojekyll').write_text('', encoding='utf-8')
    print(f'Built {count} validated static files for {base}.')


if __name__ == '__main__':
    build()
