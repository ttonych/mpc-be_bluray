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

def apply_native(source):
    # bluray.c is touched by both patches. Verify a complete known stage before
    # applying anything, rather than accepting individual mismatched files.
    components = ['mouse-page', 'playmark-seek']
    patches = []
    stages = [{}]
    for component in components:
        patch = ROOT / f'patches/libbluray-1.5.0-{component}.patch'
        manifest = json.loads(patch.with_suffix('.json').read_text(encoding='utf-8'))
        if digest(patch.read_bytes()) != manifest['patch_sha256']:
            raise SystemExit('Local patch checksum mismatch; review the patch before rebuilding.')
        before = stages[-1]
        for name, hashes in manifest['files'].items():
            if name in before and before[name] != hashes['base']:
                raise SystemExit('Native patch chain checksum mismatch.')
            if name not in before:
                for stage in stages:
                    stage[name] = hashes['base']
        stages.append({**before, **{name: hashes['patched'] for name, hashes in manifest['files'].items()}})
        patches.append(patch)
    actual = {name: digest((source / name).read_bytes()) for name in stages[-1]}
    if actual not in stages:
        raise SystemExit('Source differs from known native patch stages; refusing to overwrite edits.')
    git = ['git', '-c', 'core.autocrlf=false']
    for index in range(stages.index(actual), len(patches)):
        subprocess.run([*git, 'apply', '--check', str(patches[index])], cwd=source, check=True)
        subprocess.run([*git, 'apply', str(patches[index])], cwd=source, check=True)
        for name, expected in stages[index + 1].items():
            if digest((source / name).read_bytes()) != expected:
                raise SystemExit(f'Post-apply checksum mismatch: {name}')
    print('Verified libbluray 1.5.0 + mouse-page-v1 + playmark-seek-v1.')


def export_patch(source, base_source=None):
    archive = ROOT / 'downloads/libbluray-1.5.0.tar.xz'
    if digest(archive.read_bytes()) != ARCHIVE_HASH:
        raise SystemExit('Upstream archive checksum mismatch.')
    manifest = {'upstream': '1.5.0', 'local_revision': REVISION,
                'source_sha256': ARCHIVE_HASH, 'files': {}}
    if REVISION == 'playmark-seek-v1':
        if base_source is None:
            raise SystemExit('playmark-seek export requires --base-source with verified mouse-page-v1 applied.')
        previous = json.loads((ROOT / 'patches/libbluray-1.5.0-mouse-page.json').read_text(encoding='utf-8'))
        for name, hashes in previous['files'].items():
            if digest((base_source / name).read_bytes()) != hashes['patched']:
                raise SystemExit(f'Base source does not match mouse-page-v1: {name}')
        manifest['after'] = 'mouse-page-v1'
    elif base_source is not None:
        raise SystemExit('--base-source is only supported for playmark-seek.')
    patch = ''
    with tarfile.open(archive) as tar:
        for name in FILES:
            original = ((base_source / name).read_bytes() if base_source else
                        tar.extractfile('libbluray-1.5.0/' + name).read())
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
    parser.add_argument('--component', choices=['native', 'mouse-page', 'playmark-seek', 'bdj-toggle'], default='mouse-page')
    parser.add_argument('--base-source', type=Path)
    args = parser.parse_args()
    if args.component == 'bdj-toggle':
        FILES = ['src/libbluray/bdj/java/org/havi/ui/HActionableHelper.java',
                 'src/libbluray/bdj/java/org/havi/ui/HToggleButton.java']
        PATCH = ROOT / 'patches/libbluray-1.5.0-bdj-toggle.patch'
        MANIFEST = PATCH.with_suffix('.json')
        REVISION = 'bdj-toggle-v1'
    elif args.component == 'playmark-seek':
        FILES = ['src/libbluray/bluray.c']
        PATCH = ROOT / 'patches/libbluray-1.5.0-playmark-seek.patch'
        MANIFEST = PATCH.with_suffix('.json')
        REVISION = 'playmark-seek-v1'
    elif args.component == 'native':
        if args.export or args.base_source:
            parser.error('Export an individual component, not the native patch chain.')
        apply_native(args.source)
        raise SystemExit(0)
    if args.export:
        export_patch(args.source, args.base_source)
    else:
        apply_patch(args.source)
