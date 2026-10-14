"""Package a verified optional music stage as game ZIPs and a multipart 7z bundle."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import zipfile
from PrepareOptionalMusic import sha

VERSION = 'v1.0.14'
PAYLOAD = Path('SimCopterRemake/Content/Radio/OptionalMusic')
README = '''OPTIONAL MUSIC FOR SIMCOPTER REMAKE v1.0.14 OR NEWER

1. Install the full SimCopter Remake game from:
   https://github.com/timmyty/sim-copter-remake/releases/tag/v1.0.14
2. Extract this music package completely to a folder. For the All-Games bundle,
   download EVERY numbered .7z part into one folder and open .7z.001 with 7-Zip.
3. Double-click Install-Music.cmd. Select the game folder containing the root
   SimCopterRemake.exe. The installer checks all file checksums before copying.
4. Restart the game. Open Radio (Ctrl+R) and select a game collection. Songs appear
   in the displayed order with pause, seeking, previous/next and volume controls.

The installer needs no administrator rights for a writable game folder. You can
also extract the SimCopterRemake folder beside the game's root executable. The
destination is SimCopterRemake/Content/Radio/OptionalMusic/<game>. Source Notes
are archival material and are not included in the playlist.

Command line:
  powershell -NoProfile -ExecutionPolicy Bypass -File Install-OptionalMusic.ps1 -GameDirectory "C:\\Games\\SimCopterRemake"

Download individual games OR the All-Games bundle; their music files are identical.
Reinstalling skips matching files. Differing existing files are never overwritten.
To uninstall a collection, remove only its folder inside Radio/OptionalMusic.

QUALITY AND SOURCES
No supplied song was lossily re-encoded, resampled or reduced to mono. FLAC stays
FLAC; MP3 stays MP3. Four Ogg containers were repaired without re-encoding their
audio packets, with identical independently decoded PCM; untouched originals are
in Source Notes. Three MIDI-only originals include clearly labelled GeneralUser
GS renditions and the original MIDI sources.

Built-in SimCopter recordings are excluded. Different arrangements, platform
versions, loop lengths and uncertain matches remain labelled alternatives.
Spoken commercials, news, company logos and weather effects are excluded from
music playlists. The complete original SimEverything archive is a SEPARATE
preservation download, including its original names, artwork and spoken material.

Sources and credits:
https://community.simtropolis.com/forums/topic/48504-comprehensive-simcity-music-collection-version-32/
https://archive.org/details/SimEverything
Each collection.json records provenance, hashes and audio details. Soundtracks
remain the work of their original composers, publishers and credited archivists.

Some complete theme-park suites run about 38 minutes. The game decodes the current
song in the background; these long suites temporarily use more memory.
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stage', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--preservation-source', type=Path)
    args = parser.parse_args()
    stage, output = args.stage.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    source = stage / PAYLOAD
    games = sorted(p for p in source.iterdir() if p.is_dir())
    if not games: raise ValueError('No staged game collections')
    all_entries = []
    catalog = []
    common = {'README-MUSIC.txt': README.encode('utf-8')}
    for name in ('Install-Music.cmd', 'Install-OptionalMusic.ps1'):
        common[name] = Path(__file__).with_name(name).read_bytes()
    for game in games:
        manifest = json.loads((game / 'collection.json').read_text(encoding='utf-8'))
        for track in manifest['tracks']:
            if sha(game / track['file']) != track['sha256']: raise ValueError('Staged checksum mismatch')
        files = sorted(p for p in game.rglob('*') if p.is_file())
        entries = [{'path': p.relative_to(source).as_posix(), 'bytes': p.stat().st_size, 'sha256': sha(p)} for p in files]
        all_entries.extend(entries)
        package = dict(schema=1, minimum_game_version='1.0.14', games=[game.name], files=entries)
        slug = re.sub(r'[^A-Za-z0-9]+', '-', game.name).strip('-')
        archive = output / f'SimCopterRemake-{VERSION}-Music-{slug}.zip'
        with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=1) as z:
            for p in files: z.write(p, p.relative_to(stage).as_posix())
            for name, data in common.items(): z.writestr(name, data)
            z.writestr('music-package.json', json.dumps(package, indent=2, ensure_ascii=False).encode('utf-8'))
        with zipfile.ZipFile(archive) as z:
            if z.testzip(): raise ValueError('ZIP CRC failed: '+archive.name)
            for entry in entries:
                if hashlib.sha256(z.read((PAYLOAD / entry['path']).as_posix())).hexdigest() != entry['sha256']:
                    raise ValueError('ZIP bytes mismatch')
        catalog.append(dict(game=game.name, tracks=len(manifest['tracks']), archive=archive.name,
            bytes=archive.stat().st_size, sha256=sha(archive)))
        print('Packaged',game.name,len(manifest['tracks']),flush=True)
    for name, data in common.items(): (stage / name).write_bytes(data)
    (stage / 'music-package.json').write_text(json.dumps(dict(schema=1, minimum_game_version='1.0.14',
        games=[p.name for p in games], files=all_entries), indent=2, ensure_ascii=False), encoding='utf-8')
    bundle = output / f'SimCopterRemake-{VERSION}-Music-All-Games.7z'
    subprocess.run(['C:/Program Files/7-Zip/7z.exe', 'a', '-t7z', '-mx=1', '-mmt=4', '-v1800m', '-bsp0',
        str(bundle), '.'], cwd=stage, check=True)
    subprocess.run(['C:/Program Files/7-Zip/7z.exe', 't', '-bsp0', str(bundle)+'.001'], check=True)
    if args.preservation_source:
        # Binary split only: concatenating these parts exactly reproduces the user-supplied
        # original 7z. This also opens directly as a multipart archive in 7-Zip.
        original = args.preservation_source.resolve()
        part_size = 1800 * 1024 * 1024
        count = 0
        digest = hashlib.sha256()
        with original.open('rb') as f:
            while block := f.read(4 * 1024 * 1024):
                if digest is None: raise AssertionError()
                if count % part_size == 0:
                    if count: out.close()
                    out = (output / ('SimEverything-Original-Preservation.7z.'+f'{count//part_size+1:03d}')).open('wb')
                out.write(block); digest.update(block); count += len(block)
        out.close()
        assert digest.hexdigest() == sha(original)
        subprocess.run(['C:/Program Files/7-Zip/7z.exe', 't', '-bsp0',
            str(output / 'SimEverything-Original-Preservation.7z.001')], check=True)
    result = dict(version=VERSION, games=catalog, bundle_parts=[p.name for p in sorted(output.glob('*All-Games.7z.*'))])
    (output / 'Music-Downloads.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print('Verified',len(games),'game archives and complete bundles',flush=True)


if __name__ == '__main__': main()
