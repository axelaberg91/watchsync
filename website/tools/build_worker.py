#!/usr/bin/env python3
# SPDX-License-Identifier: CC-BY-NC-4.0
"""Embed only audited static assets in a small Cloudflare Worker module."""
from pathlib import Path
import base64,hashlib,json,mimetypes,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
subprocess.run([sys.executable,str(ROOT/'tools/validate_site.py')],check=True)
files={}
for p in sorted((ROOT/'site').rglob('*')):
    if not p.is_file() or p.name in ('_headers','.nojekyll'):continue
    assert 'downloads' not in p.relative_to(ROOT/'site').parts, 'Large downloads must use public snapshot URLs'
    data=p.read_bytes()
    mime=mimetypes.guess_type(p.name)[0] or 'application/octet-stream'
    if p.suffix in ('.html','.css','.js','.json','.txt'):mime+='; charset=utf-8'
    files['/'+p.relative_to(ROOT/'site').as_posix()]={'hash':hashlib.sha256(data).hexdigest()[:32],'size':len(data),'mime':mime,'base64':base64.b64encode(data).decode()}
assert sum(x['size'] for x in files.values())<750000, 'Review the hosting approach before growing this embedded site'
template=(ROOT/'src/worker.js').read_text(encoding='utf-8')
module=template.replace('__WATCHSYNC_ASSETS__',json.dumps(files,separators=(',',':')))
out=ROOT/'dist';out.mkdir(exist_ok=True)
(out/'worker.mjs').write_text(module,encoding='utf-8')
print(f'Built {len(files)} bounded static assets into {len(module.encode())} bytes.')
