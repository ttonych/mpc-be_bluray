"""Export or strictly apply a versioned local libbluray patch.

Normal builds only apply/verify an existing patch. --export is a maintainer
operation after source edits and must be followed by the component tests.
"""
import argparse
import difflib
import hashlib
import json
import subprocess
import tarfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ARCHIVE_HASH = 'f676408e91a5d321abf8b8d4dfdae36205c297dab5c54c3ec519639025f474a2'
FILES = ['src/libbluray/bluray.c', 'src/libbluray/bluray.h',
         'src/libbluray/decoders/graphics_controller.c',
         'src/libbluray/decoders/graphics_controller.h']
PATCH = ROOT / 'patches/libbluray-1.5.0-mouse-page.patch'
MANIFEST = PATCH.with_suffix('.json')
REVISION = 'mouse-page-v1'

def digest(data):
    return hashlib.sha256(data).hexdigest()

def apply_patch(source):
    manifest = json.loads(MANIFEST.read_text(encoding='utf-8'))
    if digest(PATCH.read_bytes()) != manifest['patch_sha256']:
        raise SystemExit('Local patch checksum mismatch; review the patch before rebuilding.')
    states = []
    for name, hashes in manifest['files'].items():
        actual = digest((source / name).read_bytes())
        states.append('patched' if actual == hashes['patched'] else
                      'base' if actual == hashes['base'] else 'modified')
    if all(state == 'patched' for state in states):
        print(f'Verified libbluray 1.5.0 + {REVISION} (already applied).')
        return
    if not all(state == 'base' for state in states):
        raise SystemExit('Source differs from both the pinned base and patched files; refusing to overwrite edits.')
    git = ['git', '-c', 'core.autocrlf=false']
    subprocess.run([*git, 'apply', '--check', str(PATCH)], cwd=source, check=True)
    subprocess.run([*git, 'apply', str(PATCH)], cwd=source, check=True)
    for name, hashes in manifest['files'].items():
        if digest((source / name).read_bytes()) != hashes['patched']:
            raise SystemExit(f'Post-apply checksum mismatch: {name}')
    print(f'Applied and verified libbluray 1.5.0 + {REVISION}.')

def export_patch(source):
    archive = ROOT / 'downloads/libbluray-1.5.0.tar.xz'
    if digest(archive.read_bytes()) != ARCHIVE_HASH:
        raise SystemExit('Upstream archive checksum mismatch.')
    manifest = {'upstream': '1.5.0', 'local_revision': REVISION,
                'source_sha256': ARCHIVE_HASH, 'files': {}}
    patch = ''
    with tarfile.open(archive) as tar:
        for name in FILES:
            original = tar.extractfile('libbluray-1.5.0/' + name).read()
            modified = (source / name).read_bytes()
            manifest['files'][name] = {'base': digest(original), 'patched': digest(modified)}
            patch += ''.join(difflib.unified_diff(original.decode().splitlines(True),
                modified.decode().splitlines(True), fromfile='a/' + name, tofile='b/' + name))
    PATCH.parent.mkdir(exist_ok=True)
    PATCH.write_bytes(patch.encode())
    manifest['patch_sha256'] = digest(PATCH.read_bytes())
    MANIFEST.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(PATCH)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT / 'vendor/libbluray-1.5.0')
    parser.add_argument('--export', action='store_true')
    parser.add_argument('--component', choices=['mouse-page', 'bdj-toggle'], default='mouse-page')
    args = parser.parse_args()
    if args.component == 'bdj-toggle':
        FILES = ['src/libbluray/bdj/java/org/havi/ui/HActionableHelper.java',
                 'src/libbluray/bdj/java/org/havi/ui/HToggleButton.java']
        PATCH = ROOT / 'patches/libbluray-1.5.0-bdj-toggle.patch'
        MANIFEST = PATCH.with_suffix('.json')
        REVISION = 'bdj-toggle-v1'
    if args.export:
        export_patch(args.source)
    else:
        apply_patch(args.source)
