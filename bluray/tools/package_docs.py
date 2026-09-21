"""Render the reviewed user Markdown as self-contained, linked HTML for the ZIP."""
import html
from html.parser import HTMLParser
import posixpath
import re
from urllib.parse import quote, unquote, urlsplit, urlunsplit

import markdown


PAGES = {
    'README.md': 'Readme.html',
    'README.ru.md': 'Readme.ru.html',
    'docs/en/USAGE.md': 'docs/en/USAGE.html',
    'docs/ru/USAGE.md': 'docs/ru/USAGE.html',
    'docs/en/CHANGELOG.md': 'docs/en/CHANGELOG.html',
    'docs/ru/CHANGELOG.md': 'docs/ru/CHANGELOG.html',
}
EXTRAS = {
    'LICENSE.txt': 'licenses/MPC-BE-License.txt',
    'docs/Authors.txt': 'Authors.txt',
    'bluray/patches/README.md': 'patches/README.md',
    'bluray/patches/README.en.md': 'patches/README.en.md',
}
CSS = '''body{font:16px/1.55 system-ui,Segoe UI,sans-serif;max-width:900px;
margin:32px auto;padding:0 24px;color:#222;background:#fff}h1,h2,h3{line-height:1.25}
h2{margin-top:1.8em;border-bottom:1px solid #ddd;padding-bottom:.25em}
a{color:#075db5}code{font:90% Consolas,monospace;background:#f3f4f6;padding:2px 4px}
pre{padding:12px;overflow:auto;background:#f3f4f6}table{border-collapse:collapse;
width:100%;margin:1em 0}td,th{border:1px solid #ccc;padding:8px;text-align:left}
th{background:#f3f4f6}li{margin:.35em 0}nav{border-bottom:1px solid #ddd;padding-bottom:12px}
@media print{body{margin:0;max-width:none}a{color:inherit}}'''


def rewrite_links(text, source, destination, root, commit):
    mapping = PAGES | EXTRAS

    def replace(match):
        target = match.group(2)
        url = urlsplit(target)
        if url.scheme or url.netloc:
            return match.group(0)
        path = (root / source).parent / unquote(url.path) if url.path else root / source
        path = path.resolve()
        if not path.is_relative_to(root) or not path.is_file():
            raise ValueError(f'{source}: invalid document link {target}')
        name = path.relative_to(root).as_posix()
        if name in mapping:
            link = posixpath.relpath(mapping[name], posixpath.dirname(destination) or '.')
            target = urlunsplit(('', '', quote(link), url.query, url.fragment))
        else:
            target = f'https://github.com/ttonych/mpc-be_bluray/blob/{commit}/{quote(name)}'
            if url.fragment:
                target += '#' + url.fragment
        return f'{match.group(1)}({target})'

    return re.sub(r'(\[[^\]]*\])\(([^\s)]+)\)', replace, text)


class Links(HTMLParser):
    def __init__(self, text):
        super().__init__()
        self.targets, self.ids = [], set()
        self.feed(text)

    def handle_starttag(self, tag, attrs):
        values = dict(attrs)
        if 'id' in values:
            self.ids.add(values['id'])
        if tag == 'a' and 'href' in values:
            self.targets.append(values['href'])


def verify_links(data):
    pages = {name: Links(content.decode('utf-8')) for name, content in data.items()
             if name.endswith('.html')}
    pages.update({name: Links(markdown.markdown(content.decode('utf-8-sig'), extensions=['toc']))
                  for name, content in data.items() if name.endswith('.md')})
    for source, page in pages.items():
        for target in page.targets:
            url = urlsplit(target)
            if url.scheme or url.netloc:
                continue
            name = posixpath.normpath(posixpath.join(posixpath.dirname(source), unquote(url.path))) if url.path else source
            if name not in data:
                raise ValueError(f'{source}: missing packaged link {target}')
            if url.fragment and name in pages and unquote(url.fragment) not in pages[name].ids:
                raise ValueError(f'{source}: missing packaged anchor {target}')


def add_documents(data, root, commit):
    if not re.fullmatch(r'[0-9a-f]{40}', commit):
        raise ValueError('Expected a full build commit for source links.')
    data['Authors.txt'] = (root / 'docs/Authors.txt').read_bytes()
    for source, destination in EXTRAS.items():
        if source.endswith('.md'):
            text = (root / source).read_text(encoding='utf-8-sig')
            data[destination] = rewrite_links(text, source, destination, root, commit).encode('utf-8')
    for source, destination in PAGES.items():
        text = (root / source).read_text(encoding='utf-8-sig')
        text = rewrite_links(text, source, destination, root, commit)
        body = markdown.markdown(text, extensions=['tables', 'fenced_code', 'toc'],
            extension_configs={'toc': {'slugify': lambda value, separator:
                re.sub(r'[^\w\- ]', '', value.lower()).replace(' ', '-')}})
        russian = '.ru.' in source or '/ru/' in source
        lang, label, overview = ('ru', 'О проекте и быстрый запуск', 'Readme.ru.html') if russian else (
            'en', 'Overview and quick start', 'Readme.html')
        link = posixpath.relpath(overview, posixpath.dirname(destination) or '.')
        title = html.escape(text.splitlines()[0].lstrip('# '))
        document = f'<!doctype html>\n<html lang="{lang}"><meta charset="utf-8">\n'
        document += f'<meta name="viewport" content="width=device-width, initial-scale=1">\n'
        document += f'<title>{title} — MPC-BE Blu-ray</title><style>{CSS}</style>\n'
        document += f'<body><nav><a href="{link}">{label}</a></nav>\n{body}\n</body></html>\n'
        data[destination] = document.encode('utf-8')
    verify_links(data)
