"""Exercise the real Windows installer in an explicitly supplied scratch directory."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('scratch', type=Path)
    args = parser.parse_args()
    root = args.scratch.resolve()
    if root.exists(): raise ValueError('Use a new scratch directory')
    package, game = root / 'package', root / 'game'
    payload = Path('SimCopterRemake/Content/Radio/OptionalMusic')
    (package / payload / 'Test Game').mkdir(parents=True)
    game.mkdir()
    (game / 'SimCopterRemake.exe').write_bytes(b'Fixture only; never launched')
    (game / 'personal-save.sav').write_bytes(b'untouched save')
    script = Path(__file__).with_name('Install-OptionalMusic.ps1')
    shutil.copy2(script, package / script.name)
    files = []
    for name, data in [('Test Game/001 Title.mp3', b'audio fixture'),
                       ('Test Game/Source Notes/Credits.txt', b'credits fixture')]:
        target = package / payload / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        files.append({'path': name, 'sha256': hashlib.sha256(data).hexdigest()})
    manifest = {'schema': 1, 'minimum_game_version': '1.0.14', 'files': files}
    def write_manifest(value):
        (package / 'music-package.json').write_text(json.dumps(value), encoding='utf-8')
    def version(value):
        (game / 'BUILD_INFO.json').write_text(json.dumps({'version': value}), encoding='utf-8')
    def run(success):
        result = subprocess.run(['powershell.exe', '-NoLogo', '-NoProfile', '-ExecutionPolicy', 'Bypass',
            '-File', str(package / script.name), '-GameDirectory', str(game)], capture_output=True, text=True)
        assert (result.returncode == 0) == success, result.stdout + result.stderr
        return ' '.join((result.stdout + result.stderr).split())
    write_manifest(manifest)
    version('v1.0.13')
    assert 'requires' in run(False)
    assert not (game / payload).exists()
    version('v1.0.14')
    hostile = dict(manifest, files=[dict(files[0], path='../../outside.mp3')])
    write_manifest(hostile)
    assert 'Invalid package path' in run(False)
    assert not (game / payload).exists()
    write_manifest(manifest)
    track = package / payload / files[0]['path']
    track.write_bytes(b'damaged download')
    assert 'damaged package' in run(False)
    assert not (game / payload).exists()
    track.write_bytes(b'audio fixture')
    assert 'Installed 2 files' in run(True)
    assert 'Installed 0 files; 2 files were already installed' in run(True)
    for entry in files:
        assert hashlib.sha256((game / payload / entry['path']).read_bytes()).hexdigest() == entry['sha256']
    custom = game / payload / files[0]['path']
    custom.write_bytes(b'custom user audio')
    assert 'existing file differs' in run(False)
    assert custom.read_bytes() == b'custom user audio'
    assert (game / 'personal-save.sav').read_bytes() == b'untouched save'
    results = {'passed': ['minimum version', 'path containment', 'corrupt download preflight',
        'installation and SHA-256', 'idempotent reinstall', 'custom file protection', 'unrelated save preservation']}
    (root / 'results.json').write_text(json.dumps(results, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(results, indent=2))


if __name__ == '__main__': main()
