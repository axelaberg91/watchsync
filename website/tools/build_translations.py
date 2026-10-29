#!/usr/bin/env python3
# SPDX-License-Identifier: CC-BY-NC-4.0
"""Generate Swedish HTML from reviewed translations; reject missing copy."""
from html import escape
from html.parser import HTMLParser
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1]
SITE = ROOT / 'site'
VOID = {'area', 'base', 'br', 'col', 'embed', 'hr', 'img', 'input',
        'link', 'meta', 'param', 'source', 'track', 'wbr'}


class SwedishPage(HTMLParser):
    def __init__(self, translations, page):
        super().__init__(convert_charrefs=True)
        self.translations, self.page = translations, page
        self.output, self.stack = [], []

    def translate(self, value):
        key = value.strip()
        if not key or not any(c.isalpha() for c in key):
            return value
        assert key in self.translations, f'Missing Swedish translation in {self.page}: {key}'
        start = len(value) - len(value.lstrip())
        end = len(value.rstrip())
        translated = self.translations[key]
        leading = '' if translated[0] in '.,;:!?-' else value[:start]
        if key[0] in ',;' and translated[0].isalpha() and not leading:
            leading = ' '
        return leading + translated + value[end:]

    def handle_starttag(self, tag, attrs):
        data = dict(attrs)
        result = []
        for key, value in attrs:
            if value is None:
                result.append(key)
                continue
            if key in ('alt', 'aria-label', 'placeholder') or (
                    tag == 'meta' and data.get('name') == 'description' and key == 'content'):
                value = self.translate(value)
            if tag == 'html' and key == 'lang':
                value = 'sv'
            if tag == 'base' and key == 'href':
                value = '/sv/'
            if key in ('href', 'src'):
                if data.get('data-language') == 'en':
                    value = '../' + self.page + '?lang=en'
                elif data.get('data-language') == 'sv':
                    value = self.page + '?lang=sv'
                elif data.get('hreflang') == 'en':
                    value = '../' + self.page
                elif data.get('hreflang') == 'sv':
                    value = self.page
                elif value.startswith('assets/'):
                    value = 'diy-wiring.svg' if value == 'assets/diy-wiring.svg' else '../' + value
            if key == 'aria-current' and data.get('data-language'):
                continue
            if tag == 'input' and data.get('name') == 'language' and key == 'value':
                value = 'sv'
            output_key = 'viewBox' if tag == 'svg' and key == 'viewbox' else key
            result.append(f'{output_key}="{escape(value, quote=True)}"')
        if data.get('data-language') == 'sv':
            result.append('aria-current="true"')
        self.output.append('<' + tag + (' ' + ' '.join(result) if result else '') + '>')
        if tag not in VOID:
            self.stack.append(tag)

    def handle_endtag(self, tag):
        self.output.append(f'</{tag}>')
        assert self.stack and self.stack[-1] == tag, f'Unbalanced HTML in {self.page}: {tag}'
        self.stack.pop()

    def handle_data(self, value):
        if not any(tag in self.stack for tag in ('code', 'script', 'style')):
            value = self.translate(value)
        self.output.append(value if any(tag in self.stack for tag in ('script', 'style')) else escape(value, quote=False))

    def handle_decl(self, decl):
        self.output.append('<!' + decl + '>')

    def handle_comment(self, value):
        self.output.append('<!--' + value + '-->')


def generate():
    translations = json.loads((ROOT / 'translations/sv.json').read_text(encoding='utf-8'))
    destination = SITE / 'sv'
    destination.mkdir(exist_ok=True)
    for source in sorted(SITE.glob('*.html')):
        parser = SwedishPage(translations, source.name)
        parser.feed(source.read_text(encoding='utf-8'))
        assert not parser.stack, f'Unclosed HTML in {source.name}'
        (destination / source.name).write_text(''.join(parser.output), encoding='utf-8')
    parser = SwedishPage(translations, 'diy-wiring.svg')
    parser.feed((SITE / 'assets/diy-wiring.svg').read_text(encoding='utf-8'))
    (destination / 'diy-wiring.svg').write_text(''.join(parser.output), encoding='utf-8')


if __name__ == '__main__':
    generate()
