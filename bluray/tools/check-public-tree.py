"""Check new publication content locally, including blobs removed in later commits.

This complements review; pattern scanning cannot prove that arbitrary text is anonymous.
Existing upstream history and its author attribution are preserved.
"""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[2]
BASE = json.loads((ROOT / 'bluray/versions.json').read_text(encoding='utf-8'))['mpc_be']['commit']
CANVAS = 'bluray/assets/bdj-canvas.mkv'
CANVAS_SHA = '25e52166d32c2d1b72e0876937bbb0765a491e6396ffcb255aff6f6e35d9f1f0'
PRIVATE_EXTENSIONS = {'.ini', '.mpcpl', '.log', '.dmp', '.pdb', '.zip', '.7z', '.exe', '.dll', '.jar'}
TEXT_PATTERNS = [
    ('personal or machine-specific absolute path', re.compile(r'[A-Za-z]:[\\/]+(?:Users|dev|temp|Ghidra\d*)[\\/]', re.I)),
    ('GitHub token', re.compile(r'\b(?:gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,})')),
    ('private key', re.compile(r'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----')),
]

def inspect(name, data):
    path = PurePosixPath(name)
    errors = []
    if path.suffix.lower() in PRIVATE_EXTENSIONS or any(part in ('bdj-data', 'maintenance', 'diagnostics') for part in path.parts):
        errors.append('private data or generated artifact path')
    if name.startswith(('bluray/build/', 'bluray/out/', 'bluray/downloads/', 'bluray/vendor/', 'bluray/tools/msys/', 'bluray/tools/.venv/')):
        errors.append('build dependency/cache must not be tracked')
    if name == CANVAS:
        if hashlib.sha256(data).hexdigest() != CANVAS_SHA:
            errors.append('the generated black canvas does not match its approved hash')
        return errors
    if data.startswith((b'MZ', b'PK\x03\x04')) or path.suffix.lower() in {'.mkv', '.m2ts', '.mp4', '.png', '.jpg', '.jpeg'}:
        errors.append('unexpected new binary/media file')
    text = data.decode('utf-16' if data.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig', errors='replace')
    for label, pattern in TEXT_PATTERNS:
        if pattern.search(text):
            errors.append(label)
    return errors

def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])

def names(data):
    return [p.decode('utf-8') for p in data.split(b'\0') if p]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', default=BASE)
    args = parser.parse_args()
    subprocess.run(['git', '-C', str(ROOT), 'merge-base', '--is-ancestor', args.base, 'HEAD'], check=True)
    problems = []
    checked = set()
    def check(name, data, location):
        key = (name, hashlib.sha256(data).hexdigest())
        if key in checked:
            return
        checked.add(key)
        for reason in inspect(name, data):
            problems.append({'file': name, 'location': location, 'reason': reason})

    # Scan each introduced blob, not only the final diff: deleting a secret later
    # does not remove it from the history being published.
    for commit in git('rev-list', '--reverse', args.base + '..HEAD').decode().splitlines():
        paths = names(git('diff-tree', '-m', '--no-commit-id', '--name-only', '--diff-filter=ACMRT', '-r', '-z', commit))
        for name in paths:
            check(name, git('show', commit + ':' + name), commit[:12])
    paths = set(names(git('diff', '--name-only', '--diff-filter=ACMRT', '-z', args.base)))
    paths.update(names(git('ls-files', '--others', '--exclude-standard', '-z')))
    for name in sorted(paths):
        path = ROOT / name
        if path.is_symlink():
            problems.append({'file': name, 'reason': 'new symlink requires manual review'})
        elif path.is_file():
            check(name, path.read_bytes(), 'working tree')
    print(json.dumps({'blobs_checked': len(checked), 'problems': problems}, indent=2))
    return 1 if problems else 0

if __name__ == '__main__':
    raise SystemExit(main())
