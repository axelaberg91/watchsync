#!/usr/bin/env python3
# SPDX-License-Identifier: CC-BY-NC-4.0; original tooling by axelaberg91.
"""Stage release metadata and checksums; publishing remains a separate action."""
import argparse
import datetime
import hashlib
import json
import re
from pathlib import Path
from validate_site import SITE, validate

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('manifest', type=Path)
p.add_argument('--date', required=True, help='Release date, YYYY-MM-DD')
p.add_argument('--summary', required=True)
p.add_argument('--summary-sv', required=True, help='Reviewed Swedish release summary')
p.add_argument('--replace-guide-version', help='Explicitly replace this version in current HTML guides and translations')
a = p.parse_args()
datetime.date.fromisoformat(a.date)
m = json.loads(a.manifest.read_text(encoding='utf-8'))
assert m['kind'] == 'application-only' and m['containsCompiledWifiCredentials'] is False
assert m['target'] == 'M5 Atom Lite (classic ESP32), GPIO25' and m['partitionScheme'] == 'min_spiffs'
assert m['file'] == f"watchsync-{m['version']}.bin"
assert re.fullmatch(r'\d+\.\d+\.\d+',m['version']) and re.fullmatch(r'[a-f0-9]{64}',m['sha256'])
assert type(m['size']) is int and 4096 <= m['size'] <= 1966080
assert a.summary.strip() and a.summary_sv.strip(), 'Both release summaries are required'
binary = (a.manifest.parent / m['file']).read_bytes()
assert len(binary) == m['size'] and hashlib.sha256(binary).hexdigest() == m['sha256'], 'Application does not match its manifest'
config_path = SITE.parent / 'migration.json'
config = json.loads(config_path.read_text(encoding='utf-8'))
assert m['version'] == config['firmwareVersion'], 'Package does not match the planned firmware release'
repository = config['repository']
assert re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', repository)
base = f'https://github.com/{repository}/releases/'
# Include only the public download fields in the served catalog.
r = {k:m[k] for k in ('version','target','partitionScheme','size','sha256')}
r.update(date=a.date, summary=a.summary, summarySv=a.summary_sv,
         binaryUrl=base + f"download/v{m['version']}/{m['file']}",
         notesUrl=base + f"tag/v{m['version']}")
path = SITE/'releases.json'
data = json.loads(path.read_text(encoding='utf-8'))
assert not any(x['version']==m['version'] for x in data['releases']), 'Version is already registered'
assert data['repository'] == repository
data['releases'].append(r)
data['releases'].sort(key=lambda x:tuple(map(int,x['version'].split('.'))),reverse=True)
allowed = {m['file'], 'manifest.json', 'SHA256SUMS', 'README.md', 'RELEASE_NOTES.md', 'LICENSE.md', 'CC-BY-NC-4.0.txt'}
assets = []
for asset in sorted(a.manifest.parent.iterdir()):
    assert asset.is_file() and asset.name in allowed, f'Unexpected release file: {asset.name}'
    content = asset.read_bytes()
    assets.append({'name': asset.name, 'size': len(content), 'sha256': hashlib.sha256(content).hexdigest()})
assert {m['file'], 'manifest.json', 'SHA256SUMS'} <= {x['name'] for x in assets}
config['firmware'] = {'version': m['version'], 'tag': 'v' + m['version'], 'assets': assets}
config['state'] = 'staged'
updates = {path: json.dumps(data, indent=2) + '\n', config_path: json.dumps(config, indent=2) + '\n'}
if a.replace_guide_version:
    assert re.fullmatch(r'\d+\.\d+\.\d+', a.replace_guide_version)
    for guide in [*SITE.glob('*.html'), SITE.parent/'translations/sv.json']:
        updates[guide] = guide.read_text(encoding='utf-8').replace(a.replace_guide_version, m['version'])
original = {file: file.read_bytes() for file in updates}
try:
    for file, text in updates.items():
        file.write_text(text, encoding='utf-8')
    validate()
except BaseException:
    for file, content in original.items():
        file.write_bytes(content)
    raise
print('Release metadata and expected asset checksums staged. No assets were uploaded and production remains gated.')
