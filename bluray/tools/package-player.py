"""Create a candidate ZIP from verified program files and a fresh portable profile."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

component = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--allow-dirty', action='store_true', help='Local testing only; never used in public release workflows.')
args = parser.parse_args()
source = component / 'out/mpc-be-bluray-x64'
build = json.loads((source / 'build-manifest.json').read_text(encoding='utf-8-sig'))
if not build['portable_test']:
    raise SystemExit('Expected an isolated portable build.')
if build['source_dirty'] and not args.allow_dirty:
    raise SystemExit('Commit the reviewed source first; dirty builds are for local testing only.')
programs = {
    'mpc-be64.exe', 'Lang/mpcresources.ru.dll', 'bluray-4.dll', 'udfread-3.dll',
    'freetype.dll', 'libxml2.dll', 'brotlicommon.dll', 'brotlidec.dll', 'bz2.dll',
    'libpng16.dll', 'z.dll', 'iconv-2.dll', 'charset-1.dll',
    'libbluray-j2se-1.5.0.jar', 'libbluray-awt-j2se-1.5.0.jar', 'bdj-canvas.mkv',
}
recorded = {item['name']: item['sha256'] for item in build['files']}
if set(recorded) != programs:
    raise SystemExit('Unexpected program file set; review dependency changes before packaging.')
data = {}
for name in sorted(programs):
    content = (source / name).read_bytes()
    if hashlib.sha256(content).hexdigest() != recorded[name]:
        raise SystemExit('Program hash mismatch: ' + name)
    data[name] = content
license_names = {'MPC-BE-License.txt', 'libbluray.txt', 'libudfread.txt', 'freetype.txt',
                 'libxml2.txt', 'brotli.txt', 'bzip2.txt', 'libpng.txt', 'zlib.txt', 'libiconv.txt',
                 'vcpkg-port.txt'}
for name in sorted(license_names):
    path = source / 'licenses' / name
    if path.is_symlink():
        raise SystemExit('Unexpected license symlink.')
    data['licenses/' + path.name] = path.read_bytes()
for path in (component / 'patches').iterdir():
    if path.suffix in ('.patch', '.json', '.md') and path.is_file() and not path.is_symlink():
        data['patches/' + path.name] = path.read_bytes()
data['mpc-be64.ini'] = ('[Settings]\r\nBluRayMenus=1\r\nChapterMarker=1\r\nMultipleInstances=2\r\n'
    'KeepHistory=0\r\nRememberFilePos=0\r\n[OSD]\r\nShowOSD=5\r\n[Audio]\r\nVolume=25\r\n'
    '[WebServer]\r\nEnableWebServer=0\r\n[Video]\r\nVideoRenderer=7\r\n'
    '[PortableTest]\r\nFirstRunComplete=0\r\n').encode('utf-16')
data['build-manifest.json'] = json.dumps(build, indent=2).encode()
manifest = {'files': {name: hashlib.sha256(content).hexdigest() for name, content in sorted(data.items())}}
data['package-manifest.json'] = json.dumps(manifest, indent=2).encode()
version = build['upstream']['version'] + '-bluray.' + str(build['bluray_revision'])
name = 'mpc-be_bluray-' + version + ('-local' if args.allow_dirty else '') + '-x64'
folder = component / 'out/packages'
folder.mkdir(parents=True, exist_ok=True)
target = folder / (name + '.zip')
with zipfile.ZipFile(target, 'x', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
    for path, content in sorted(data.items()):
        z.writestr(name + '/' + path, content)
with zipfile.ZipFile(target) as z:
    assert z.testzip() is None
    for path, digest in manifest['files'].items():
        assert hashlib.sha256(z.read(name + '/' + path)).hexdigest() == digest
digest = hashlib.sha256(target.read_bytes()).hexdigest()
target.with_suffix('.zip.sha256').write_text(digest + '  ' + target.name + '\n', encoding='ascii')
print(json.dumps({'archive': target.name, 'sha256': digest, 'files': len(data)}, indent=2))
