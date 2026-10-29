#!/usr/bin/env python3
# SPDX-License-Identifier: CC-BY-NC-4.0
# Original packaging tooling by axelaberg91.
"""Package reviewed enclosure parts outside Git without changing geometry."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
PARTS = {
    'watchsync-base.stl': '510495268afef128b4c80eec349165d87ad78c11b7e78eb9dbc293c3e7a2aa41',
    'watchsync-lid.stl': '49c7923bc632c0d79bab3e28402e75348f222d6fe5ea34c8755b93c92a57f4a1',
    'watchsync-bobbin.stl': '3870dadf463d2ced89bcd7519601e0f0d3cafa1ab84285e603fb9f525edcaba6',
}


def git(*args):
    return subprocess.check_output(['git', '-c', f'safe.directory={ROOT.as_posix()}',
                                    '-C', str(ROOT), *args], text=True).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--parts', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output == ROOT or ROOT in output.parents:
        raise SystemExit('Release output must remain outside the repository.')
    if output.exists() and any(output.iterdir()):
        raise SystemExit('Release output must be new or empty; existing files are preserved.')
    if git('status', '--porcelain', '--untracked-files=normal'):
        raise SystemExit('Commit the reviewed sources before packaging.')
    commit = git('rev-parse', 'HEAD')
    for name in PARTS:
        if not (args.parts/name).is_file() or (args.parts/name).is_symlink():
            raise SystemExit('Missing reviewed STL input: '+name)
        if hashlib.sha256((args.parts/name).read_bytes()).hexdigest() != PARTS[name]:
            raise SystemExit('STL bytes differ from the reviewed hardware-v14 inputs: '+name)
    output.mkdir(parents=True, exist_ok=True)
    for name in PARTS:
        shutil.copyfile(args.parts/name, output/name)
    shutil.copyfile(ROOT/'hardware/watchsync-enclosure.scad', output/'watchsync-enclosure.scad')
    shutil.copyfile(ROOT/'LICENSE.md', output/'LICENSE.md')
    shutil.copyfile(ROOT/'LICENSES/CC-BY-NC-4.0.txt', output/'CC-BY-NC-4.0.txt')
    notes = ('# Watchsync enclosure v14\n\n'
             'Bottom plate v14, unchanged upper shell and coil former.\n'
             'The supplied STL geometry is preserved from the reviewed published parts.\n'
             'This packaging change does not establish physical print or device validation.\n'
             'See the website for the Prusa CORE One / PLA printing and assembly guide.\n\n'
             'Original design and packaging by axelaberg91, CC BY-NC 4.0.\n'
             'Third-party rights and the root licensing notice remain applicable.\n')
    (output/'RELEASE_NOTES.md').write_text(notes, encoding='utf-8', newline='\n')
    files = {p.name: {'size': p.stat().st_size,
                     'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}
             for p in sorted(output.iterdir()) if p.is_file()}
    manifest = {'version': 'hardware-v14', 'sourceCommit': commit,
                'physicalPrintVerified': False, 'geometryModified': False, 'files': files}
    (output/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    archive = output/'watchsync-hardware-v14.zip'
    with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as bundle:
        for path in sorted(output.iterdir()):
            if not path.is_file() or path == archive:
                continue
            entry = zipfile.ZipInfo(path.name, date_time=(1980, 1, 1, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.external_attr = 0o100644 << 16
            bundle.writestr(entry, path.read_bytes())
    sums = ''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n'
                   for p in sorted(output.iterdir()) if p.is_file())
    (output/'SHA256SUMS').write_text(sums, encoding='utf-8', newline='\n')
    subprocess.run([sys.executable, str(ROOT/'tools/check_public_tree.py'), '--artifacts',
                    str(output), '--artifacts-only'], check=True)
    print(f'Hardware release package: {output}')


if __name__ == '__main__':
    main()
