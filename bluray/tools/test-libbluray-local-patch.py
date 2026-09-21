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
parser.add_argument('--component', choices=['mouse-page', 'bdj-toggle'], default='mouse-page')
args = parser.parse_args()
target = root / 'diagnostics' / 'fixtures' / ('patch-rebuild-' + args.component + '-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
manifest = json.loads((root / ('patches/libbluray-1.5.0-' + args.component + '.json')).read_text())
with tarfile.open(root / 'downloads/libbluray-1.5.0.tar.xz') as tar:
    for name in manifest['files']:
        path = target / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(tar.extractfile('libbluray-1.5.0/' + name).read())
command = [sys.executable, str(root / 'tools/libbluray-local-patch.py'), '--source', str(target), '--component', args.component]
subprocess.run(command, check=True)
subprocess.run(command, check=True)
path = target / next(iter(manifest['files']))
patched = path.read_bytes()
path.write_bytes(patched + b'\n/* unexpected local edit */\n')
result = subprocess.run(command, capture_output=True, text=True)
assert result.returncode != 0 and 'refusing to overwrite' in result.stderr, result
assert path.read_bytes() == patched + b'\n/* unexpected local edit */\n'
path.write_bytes(patched)
print('PASS: clean apply, repeated apply, preservation of unexpected edits.')
