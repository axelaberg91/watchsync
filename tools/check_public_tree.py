#!/usr/bin/env python3
# SPDX-License-Identifier: CC-BY-NC-4.0
"""Check source, release bytes and ZIP metadata without printing matched values.

This bounded heuristic catches recognized disclosure patterns. It does not prove
that every possible secret or personal detail has been removed.
"""
from __future__ import annotations

import argparse
import io
import os
from pathlib import Path
import re
import subprocess
import sys
import zipfile

MAX_FILE = 64 * 1024 * 1024
MAX_ARCHIVE_TOTAL = 128 * 1024 * 1024
MAX_ARCHIVE_MEMBERS = 512
MAX_ARCHIVE_DEPTH = 3
GENERATED_DIRS = {'.git', 'node_modules', '__pycache__', '.wrangler', 'dist'}
PRIVATE_NAMES = {'secret.h', 'secrets.h', 'secrets_example.h', '.env', '.dev.vars',
                 '.ds_store', 'thumbs.db', 'credentials', 'id_rsa', 'id_ed25519'}
RELEASE_SUFFIXES = {'.bin', '.elf', '.hex', '.map', '.log', '.stl', '.3mf', '.zip', '.bak'}
PATTERNS = {
    'private key': rb'-----BEGIN (?:RSA |EC |OPENSSH |DSA |ENCRYPTED |PGP )?PRIVATE KEY(?: BLOCK)?-----',
    'GitHub token': rb'\b(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{40,})\b',
    'AWS access key': rb'\b(?:AKIA|ASIA)[A-Z0-9]{16}\b',
    'Slack token': rb'\bxox[baprs]-[A-Za-z0-9-]{20,}',
    'local user path': rb'(?i)(?:[A-Z]:[\\/]+Users[\\/]+|/Users/|/home/)(?!<|\{|\$)[A-Za-z0-9_.-]{2,}[\\/]',
}
EMAIL = re.compile(rb'(?<![A-Za-z0-9_.+%-])[A-Za-z0-9_.+%-]{3,}@[A-Za-z0-9-]{2,}(?:\.[A-Za-z0-9-]+)*\.[A-Za-z]{2,24}(?![A-Za-z0-9])')
# Reserved example domains and GitHub privacy addresses are safe to publish.
SAFE_EMAIL_DOMAINS = {'example.com', 'example.org', 'example.net', 'users.noreply.github.com'}
ASSIGNMENT = re.compile(
    rb'(?im)\b(?:WIFI_SSID|WIFI_PASS(?:WORD)?|AWS_SECRET_ACCESS_KEY|CLOUDFLARE_API_(?:TOKEN|KEY)|'
    rb'GITHUB_TOKEN|API_SECRET|API_KEY|API_TOKEN|CLIENT_SECRET|ACCESS_TOKEN)\b[\"\']?\s*'
    rb'(?:(?:=|:)\s*)?[\"\']([^\"\'\r\n]{3,})[\"\']')
PLACEHOLDERS = {b'YourSSID', b'YourPassword', b'YOUR_SSID', b'YOUR_PASSWORD',
                b'example', b'placeholder', b'REPLACE_ME'}


class Review:
    def __init__(self):
        self.errors: list[str] = []
        self.files = 0
        self.members = 0
        self.archive_bytes = 0

    def fail(self, location: str, category: str):
        # Deliberately omit the matched text, credentials and source snippets.
        self.errors.append(f'{location}: {category}')

    def scan_bytes(self, body: bytes, location: str, depth: int = 0):
        if len(body) > MAX_FILE:
            self.fail(location, 'file exceeds the review size limit')
            return
        for category, pattern in PATTERNS.items():
            if re.search(pattern, body):
                self.fail(location, category)
        # Windows metadata may store paths as UTF-16 rather than byte strings.
        if b'U\0s\0e\0r\0s\0' in body or b'\0U\0s\0e\0r\0s' in body:
            if re.search(PATTERNS['local user path'], body.replace(b'\0', b'')):
                self.fail(location, 'local user path in UTF-16 metadata')
        is_zip = body.startswith(b'PK\x03\x04')
        # Compressed payloads can contain accidental email-like byte sequences.
        # Review each decompressed member instead of interpreting compressed data.
        if not is_zip:
            for match in EMAIL.finditer(body):
                domain = match.group().split(b'@', 1)[1].decode('ascii').lower()
                if domain in SAFE_EMAIL_DOMAINS or domain.endswith(('.invalid', '.example', '.test')):
                    continue
                self.fail(location, 'contact address requires an explicit attribution review')
                break
            for match in ASSIGNMENT.finditer(body):
                value = match.group(1)
                if value not in PLACEHOLDERS and not value.startswith((b'${{', b'${', b'<')):
                    self.fail(location, 'credential-like literal assignment')
                    break
        if self.has_private_image_metadata(body):
            self.fail(location, 'image contains EXIF, XMP or descriptive metadata')
        if is_zip:
            self.scan_archive(body, location, depth)

    @staticmethod
    def has_private_image_metadata(body: bytes) -> bool:
        if body.startswith(b'RIFF') and body[8:12] == b'WEBP':
            offset = 12
            while offset + 8 <= len(body):
                tag = body[offset:offset+4]
                length = int.from_bytes(body[offset+4:offset+8], 'little')
                if tag in (b'EXIF', b'XMP '):
                    return True
                offset += 8 + length + (length % 2)
        elif body.startswith(b'\x89PNG\r\n\x1a\n'):
            offset = 8
            while offset + 12 <= len(body):
                length = int.from_bytes(body[offset:offset+4], 'big')
                if body[offset+4:offset+8] in (b'eXIf', b'tEXt', b'iTXt', b'zTXt'):
                    return True
                offset += 12 + length
        elif body.startswith(b'\xff\xd8'):
            return b'Exif\0\0' in body or b'http://ns.adobe.com/xap/' in body
        return False

    def scan_archive(self, body: bytes, location: str, depth: int):
        if depth >= MAX_ARCHIVE_DEPTH:
            self.fail(location, 'archive nesting exceeds the review limit')
            return
        try:
            with zipfile.ZipFile(io.BytesIO(body)) as archive:
                self.scan_bytes(archive.comment, location + '!archive-comment', depth + 1)
                entries = archive.infolist()
                if len(entries) > MAX_ARCHIVE_MEMBERS:
                    self.fail(location, 'archive exceeds the member review limit')
                    return
                for index, entry in enumerate(entries):
                    if entry.is_dir():
                        continue
                    # Member names and comments can themselves contain private
                    # values; identify members by position without echoing them.
                    member = f'{location}!member-{index}'
                    self.scan_bytes(entry.filename.encode('utf-8'), member + '-name', depth + 1)
                    self.scan_bytes(entry.comment + entry.extra, member + '-metadata', depth + 1)
                    self.members += 1
                    if entry.flag_bits & 1:
                        self.fail(member, 'encrypted member cannot be reviewed')
                        continue
                    if entry.file_size > MAX_FILE or self.archive_bytes + entry.file_size > MAX_ARCHIVE_TOTAL:
                        self.fail(member, 'archive exceeds the decompressed review limit')
                        continue
                    self.archive_bytes += entry.file_size
                    self.scan_bytes(archive.read(entry), member, depth + 1)
        except (zipfile.BadZipFile, RuntimeError, OSError, NotImplementedError):
            self.fail(location, 'archive could not be completely reviewed')

    def scan_file(self, path: Path, label: str, artifact: bool = False):
        self.files += 1
        if path.is_symlink():
            self.fail(label, 'symbolic links are not allowed in publication inputs')
            return
        if path.name.lower() in PRIVATE_NAMES or path.name.lower().startswith(('.env.', '.dev.vars.')):
            self.fail(label, 'private/local configuration file')
        if not artifact and any(part in GENERATED_DIRS - {'.git'} for part in Path(label).parts):
            self.fail(label, 'generated dependencies or cache belong outside Git')
        if not artifact and path.suffix.lower() in RELEASE_SUFFIXES:
            self.fail(label, 'release or generated output belongs outside Git')
        try:
            if path.stat().st_size > MAX_FILE:
                self.fail(label, 'file exceeds the review size limit')
            else:
                self.scan_bytes(path.read_bytes(), label)
        except OSError:
            self.fail(label, 'file could not be read')


def git(root: Path, *args: str) -> bytes:
    return subprocess.check_output(['git', '-c', f'safe.directory={root.as_posix()}', '-C',
                                    str(root), *args], stderr=subprocess.DEVNULL)


def source_files(root: Path):
    if (root / '.git').exists():
        for name in sorted(set(git(root, 'ls-files', '--cached', '--others', '--exclude-standard', '-z').split(b'\0'))):
            if name:
                relative = Path(name.decode('utf-8'))
                yield root / relative, relative.as_posix()
    else:
        for directory, dirs, files in os.walk(root, followlinks=False):
            current = Path(directory)
            keep = []
            for name in dirs:
                path = current / name
                relative = path.relative_to(root)
                if name in GENERATED_DIRS or relative.parts == ('website', 'site', 'sv'):
                    continue
                if path.is_symlink():
                    yield path, relative.as_posix()
                else:
                    keep.append(name)
            dirs[:] = sorted(keep)
            for name in sorted(files):
                path = current / name
                yield path, path.relative_to(root).as_posix()


def scan_history(review: Review, root: Path):
    try:
        # Lower-case email placeholders read the original commit identity rather
        # than a mailmap projection that could hide an address still in Git.
        meta = git(root, 'log', '--all', '--format=%H%x00%ae%x00%ce%x00%B%x00')
        review.scan_bytes(meta, 'Git commit metadata')
        rows = git(root, 'rev-list', '--objects', '--all').decode().splitlines()
        for row in rows:
            oid, _, path = row.partition(' ')
            if git(root, 'cat-file', '-t', oid).strip() != b'blob':
                continue
            if int(git(root, 'cat-file', '-s', oid)) > MAX_FILE:
                review.fail(f'history:{oid[:12]}:{path}', 'blob exceeds the review size limit')
                continue
            review.scan_bytes(git(root, 'cat-file', '-p', oid), f'history:{oid[:12]}:{path}')
    except (OSError, subprocess.CalledProcessError):
        review.fail('Git history', 'history could not be completely reviewed')


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--artifacts', type=Path, action='append', default=[])
    parser.add_argument('--artifacts-only', action='store_true')
    parser.add_argument('--history', action='store_true', help='also scan reachable Git blobs and commit metadata')
    args = parser.parse_args(argv)
    if args.artifacts_only and not args.artifacts:
        parser.error('--artifacts-only requires --artifacts')
    review = Review()
    if not args.artifacts_only:
        for path, label in source_files(args.root.resolve()):
            review.scan_file(path, label)
    for directory in args.artifacts:
        if not directory.is_dir():
            review.fail('release artifacts', 'artifact directory is missing')
            continue
        for path in sorted(directory.rglob('*')):
            if path.is_file() or path.is_symlink():
                review.scan_file(path, 'artifacts/' + path.relative_to(directory).as_posix(), artifact=True)
    if args.history:
        scan_history(review, args.root.resolve())
    if review.errors:
        print('\n'.join('FAIL: ' + error for error in sorted(set(review.errors))), file=sys.stderr)
        return 1
    print(f'PASS: reviewed {review.files} files and {review.members} archive members for recognized disclosure patterns.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
