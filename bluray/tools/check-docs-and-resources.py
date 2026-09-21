"""Check authored Markdown links and the fork's English/Russian resources.

This checks structure and format arguments, not translation quality or UI layout.
Upstream documentation and unrelated translations are deliberately outside scope.
"""
from collections import Counter
import json
from pathlib import Path
import re
from urllib.parse import unquote, urlsplit


ROOT = Path(__file__).resolve().parents[2]
PAIRS = (
    ('README.md', 'README.ru.md'),
    ('AGENTS.md', 'AGENTS.ru.md'),
    ('ROADMAP.md', 'ROADMAP.ru.md'),
    ('docs/en/USAGE.md', 'docs/ru/USAGE.md'),
    ('docs/en/DEVELOPMENT.md', 'docs/ru/DEVELOPMENT.md'),
    ('docs/en/CHANGELOG.md', 'docs/ru/CHANGELOG.md'),
    ('bluray/patches/README.en.md', 'bluray/patches/README.md'),
)
DIALOGS = ('IDD_PPAGEBLURAY', 'IDD_BD_ADVANCED', 'IDD_BD_JAVA',
           'IDD_BD_DISCS', 'IDD_PORTABLE_SETUP')
FORMAT = re.compile(r'%[-+ #0]*\d*(?:\.\d+)?(?:I64|I32|hh|ll|h|l|z|t|j)?[diuoxXfFeEgGaAcCsSpn]')


def read_text(path):
    data = path.read_bytes()
    text = data.decode('utf-16' if data.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig')
    return text.replace('\r\n', '\n')


def anchors(text):
    counts, result = Counter(), set()
    text = re.sub(r'^```.*?^```\s*$', '', text, flags=re.M | re.S)
    for heading in re.findall(r'^#{1,6}\s+(.+?)\s*#*$', text, re.M):
        heading = re.sub(r'\[([^\]]+)\]\([^)]+\)', r'\1', heading)
        slug = re.sub(r'[^\w\- ]', '', heading.lower()).replace(' ', '-')
        result.add(slug + (f'-{counts[slug]}' if counts[slug] else ''))
        counts[slug] += 1
    return result


def check_documents(root):
    problems, texts, links = [], {}, 0
    for name in (item for pair in PAIRS for item in pair):
        path = root / name
        text = read_text(path)
        texts[name] = text
        if '\ufffd' in text or '\x00' in text:
            problems.append(f'{name}: invalid text characters')
        if len(re.findall(r'^```', text, re.M)) % 2:
            problems.append(f'{name}: unclosed code fence')
        for target in re.findall(r'\[[^\]]*\]\(([^\s)]+)\)', text):
            url = urlsplit(target)
            if url.scheme or url.netloc:
                continue
            links += 1
            dest = (path.parent / unquote(url.path)).resolve() if url.path else path
            if not dest.exists():
                problems.append(f'{name}: missing link {target}')
            elif url.fragment and dest.suffix.lower() == '.md':
                if unquote(url.fragment) not in anchors(read_text(dest)):
                    problems.append(f'{name}: missing anchor {target}')
    for en, ru in PAIRS:
        for pattern, label in ((r'^- \[([ x])\]', 'roadmap states'),
                               (r'^```powershell\n(.*?)^```', 'PowerShell examples')):
            if re.findall(pattern, texts[en], re.M | re.S) != re.findall(pattern, texts[ru], re.M | re.S):
                problems.append(f'{en} / {ru}: different {label}')
    return problems, {'documents': len(texts), 'internal_links': links}


def check_resources(root):
    problems = []
    header = read_text(root / 'src/apps/mplayerc/resource.h')
    ids = {name: int(value) for name, value in re.findall(
        r'^#define\s+(IDS_(?:BD|PT)_\w+)\s+(\d+)\s*$', header, re.M)}
    if not ids:
        return ['No Blu-ray/portable resource IDs found'], {}
    resources = []
    paths = ('src/apps/mplayerc/mplayerc.rc', 'src/apps/mpcresources/mplayerc.ru.rc')
    for name in paths:
        text = read_text(root / name)
        if '\ufffd' in text or '\x00' in text:
            problems.append(f'{name}: invalid resource encoding')
        strings = {}
        for key, value in re.findall(r'^\s*(IDS_(?:BD|PT)_\w+|\d+)\s+"(.*)"\s*$', text, re.M):
            number = int(key) if key.isdecimal() else ids.get(key)
            if number is None:
                problems.append(f'{name}: undefined string {key}')
                continue
            if not (44110 <= number <= 44403):
                continue
            if number in strings:
                problems.append(f'{name}: duplicate string {number}')
            strings[number] = value
            if not value.strip():
                problems.append(f'{name}: empty string {number}')
        for key, number in ids.items():
            if number not in strings:
                problems.append(f'{name}: missing string {key}')
        controls = {}
        for dialog in DIALOGS:
            blocks = re.findall(r'^' + dialog + r'\s+DIALOGEX\b.*?^END\s*$', text, re.M | re.S)
            if len(blocks) != 1:
                problems.append(f'{name}: expected one {dialog}')
            controls[dialog] = Counter(re.findall(r'\bIDC_(?:BD|PT)_\w+\b', '\n'.join(blocks)))
        resources.append((strings, controls))
    en, ru = resources
    if en[0].keys() != ru[0].keys():
        problems.append('RU/EN string ID sets differ')
    for number in en[0].keys() & ru[0].keys():
        left = FORMAT.findall(en[0][number].replace('%%', ''))
        right = FORMAT.findall(ru[0][number].replace('%%', ''))
        if left != right:
            problems.append(f'RU/EN format arguments differ: {number}')
    for dialog in DIALOGS:
        if en[1][dialog] != ru[1][dialog]:
            problems.append(f'RU/EN controls differ: {dialog}')
    return problems, {'localized_strings': len(en[0]), 'localized_dialogs': len(DIALOGS)}


def main():
    problems, stats = check_documents(ROOT)
    resource_problems, resource_stats = check_resources(ROOT)
    problems.extend(resource_problems)
    print(json.dumps({**stats, **resource_stats, 'problems': problems}, indent=2))
    return bool(problems)


if __name__ == '__main__':
    raise SystemExit(main())
