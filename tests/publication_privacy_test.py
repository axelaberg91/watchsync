#!/usr/bin/env python3
# SPDX-License-Identifier: CC-BY-NC-4.0
"""Regression fixtures for publication boundaries, including binary metadata."""
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest
import zipfile

spec = importlib.util.spec_from_file_location('publication_privacy', Path(__file__).resolve().parents[1] / 'tools/check_public_tree.py')
privacy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(privacy)


class PublicationPrivacyTests(unittest.TestCase):
    def review(self, data):
        review = privacy.Review()
        review.scan_bytes(data, 'fixture')
        return review.errors

    def test_binary_user_path_rejected_without_echoing_value(self):
        path = b'C:' + bytes([92]) + b'Users' + bytes([92]) + b'private-person' + bytes([92]) + b'build.cpp'
        errors = self.review(b'\x00\xe9' + path + b'\x00')
        self.assertTrue(any('local user path' in e for e in errors))
        self.assertNotIn('private-person', '\n'.join(errors))
        self.assertTrue(any('UTF-16' in e for e in self.review(path.decode().encode('utf-16le'))))
        self.assertFalse(self.review(b'/watchsync/watchsync.ino\x00/arduino/cores/esp32/file.c'))

    def test_contact_addresses_and_reserved_examples(self):
        address = b'private.person' + b'@' + b'gmail.com'
        errors = self.review(address)
        self.assertTrue(any('contact address' in e for e in errors))
        self.assertNotIn(address.decode(), '\n'.join(errors))
        self.assertFalse(self.review(b'developer' + b'@' + b'example.invalid'))
        self.assertFalse(self.review(b'123+contributor' + b'@' + b'users.noreply.github.com'))

    def test_compressed_xml_metadata_is_checked(self):
        stream = io.BytesIO()
        with zipfile.ZipFile(stream, 'w', zipfile.ZIP_DEFLATED) as archive:
            archive.writestr('Metadata/project.xml', b'<author>private.person' + b'@' + b'gmail.com</author>')
        self.assertTrue(any('member-0: contact address' in e for e in self.review(stream.getvalue())))

    def test_archive_comments_and_names_are_checked_without_echo(self):
        stream = io.BytesIO()
        address = b'private.person' + b'@' + b'gmail.com'
        with zipfile.ZipFile(stream, 'w') as archive:
            archive.comment = address
            archive.writestr(address.decode(), b'clean')
        errors = self.review(stream.getvalue())
        self.assertTrue(any('archive-comment: contact address' in e for e in errors))
        self.assertTrue(any('member-0-name: contact address' in e for e in errors))
        self.assertNotIn(address.decode(), '\n'.join(errors))

    def test_descriptive_image_metadata_rejected(self):
        payload = b'<xmp>GPS metadata</xmp>'
        chunk = b'XMP ' + len(payload).to_bytes(4, 'little') + payload
        if len(payload) % 2:
            chunk += b'\0'
        body = b'RIFF' + (len(chunk) + 4).to_bytes(4, 'little') + b'WEBP' + chunk
        self.assertTrue(any('image contains' in e for e in self.review(body)))

    def test_token_values_redacted(self):
        token = b'gh' + b'p_' + b'a' * 36
        errors = self.review(token)
        self.assertTrue(any('GitHub token' in e for e in errors))
        self.assertNotIn(token.decode(), '\n'.join(errors))

    def test_quoted_json_credential_and_encrypted_key(self):
        field = b'API_' + b'TOKEN'
        errors = self.review(b'{"' + field + b'": "nonpublic-value"}')
        self.assertTrue(any('credential-like literal' in e for e in errors))
        self.assertNotIn('nonpublic-value', '\n'.join(errors))
        pem = b'-----BEGIN ' + b'ENCRYPTED PRIVATE KEY-----'
        self.assertTrue(any('private key' in e for e in self.review(pem)))

    def test_release_binary_forbidden_in_source_allowed_as_artifact(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'watchsync-1.0.0.bin'
            path.write_bytes(b'\xe9\0clean fixture')
            source = privacy.Review()
            source.scan_file(path, path.name)
            self.assertTrue(any('outside Git' in e for e in source.errors))
            artifact = privacy.Review()
            artifact.scan_file(path, path.name, artifact=True)
            self.assertFalse(artifact.errors)

    def test_archive_bounds_fail_closed(self):
        stream = io.BytesIO()
        with zipfile.ZipFile(stream, 'w') as archive:
            for index in range(privacy.MAX_ARCHIVE_MEMBERS + 1):
                archive.writestr(f'item-{index}', b'clean')
        self.assertTrue(any('member review limit' in e for e in self.review(stream.getvalue())))


if __name__ == '__main__':
    unittest.main()
