"""Verify clean application, idempotence and refusal to overwrite local edits."""
import json
import argparse
import subprocess
import sys
import tarfile
from pathlib import Path
from datetime import datetime

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--component', choices=['native', 'mouse-page', 'bdj-toggle'], default='mouse-page')
args = parser.parse_args()
target = root / 'diagnostics' / 'fixtures' / ('patch-rebuild-' + args.component + '-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
first = 'mouse-page' if args.component == 'native' else args.component
manifest = json.loads((root / ('patches/libbluray-1.5.0-' + first + '.json')).read_text())
with tarfile.open(root / 'downloads/libbluray-1.5.0.tar.xz') as tar:
    for name in manifest['files']:
        path = target / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(tar.extractfile('libbluray-1.5.0/' + name).read())
command = [sys.executable, str(root / 'tools/libbluray-local-patch.py'), '--source', str(target), '--component', args.component]
subprocess.run(command, check=True)
subprocess.run(command, check=True)
if args.component == 'native':
    # Existing development trees may already contain the mouse patch.
    intermediate = target / 'intermediate'
    with tarfile.open(root / 'downloads/libbluray-1.5.0.tar.xz') as tar:
        for name in manifest['files']:
            path = intermediate / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(tar.extractfile('libbluray-1.5.0/' + name).read())
    middle_command = [sys.executable, str(root / 'tools/libbluray-local-patch.py'), '--source', str(intermediate)]
    subprocess.run(middle_command, check=True)
    subprocess.run([*middle_command, '--component', 'native'], check=True)
    for name in manifest['files']:
        assert (intermediate / name).read_bytes() == (target / name).read_bytes()
path = target / next(iter(manifest['files']))
patched = path.read_bytes()
path.write_bytes(patched + b'\n/* unexpected local edit */\n')
result = subprocess.run(command, capture_output=True, text=True)
assert result.returncode != 0 and 'refusing to overwrite' in result.stderr, result
assert path.read_bytes() == patched + b'\n/* unexpected local edit */\n'
path.write_bytes(patched)
print('PASS: clean apply, repeated apply, preservation of unexpected edits.')
