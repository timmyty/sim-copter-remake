"""Consolidate small verified game ZIPs into named packs without changing audio.

Run after PackageOptionalMusic.py. Input is its original assets directory; output
must be a new directory. Publish the output ZIPs and Music-Downloads.json alongside
the unchanged large ZIPs and multipart bundles. The catalog identifies exactly
which assets to retain; original individual ZIPs remain in the input directory.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import zipfile

from PackageOptionalMusic import PAYLOAD, VERSION
from PrepareOptionalMusic import sha


# Keep related collections together and never divide a game between packs.
# Compressed sizes are roughly 97-138 decimal MB for the v1.0.14 source collection.
GROUPS = [
    ('SimCity-Creator-DS-and-Wii', 'SimCity Creator DS + Wii',
     ['SimCity Creator DS', 'SimCity Creator Wii']),
    ('SimCity-3000-Unlimited-and-SimCity-4-Rush-Hour',
     'SimCity 3000 Unlimited + SimCity 4 Rush Hour',
     ['SimCity 3000 Unlimited', 'SimCity 4 Rush Hour']),
    ('SimCity-Jr-BuildIt-Social-and-Societies-Destinations',
     'SimCity Jr. + BuildIt + Social + Societies Destinations',
     ['SimCity Jr', 'SimCity BuildIt', 'SimCity Social', 'SimCity Societies Destinations']),
    ('SimFarm-SimIsle-SimLife-SimHealth-SimPark-SimSafari',
     'SimFarm + SimIsle + SimLife + SimHealth + SimPark + SimSafari',
     ['SimFarm', 'SimIsle', 'SimLife', 'SimHealth', 'SimPark', 'SimSafari']),
    ('Theme-Park-SimGolf-SimTower-SimTown',
     "Theme Park + Sid Meier's SimGolf + SimTower + SimTown",
     ['Theme Park', "Sid Meier's SimGolf", 'SimTower', 'SimTown']),
]
COMMON = ['Install-Music.cmd', 'Install-OptionalMusic.ps1', 'README-MUSIC.txt']


def regroup(source, output):
    source, output = source.resolve(), output.resolve()
    if source == output or output.exists():
        raise ValueError('Use a new output directory; keep original assets intact')
    catalog = json.loads((source / 'Music-Downloads.json').read_text(encoding='utf-8'))
    if catalog['version'] != VERSION or catalog.get('packages'):
        raise ValueError('Expected the original per-game catalog for ' + VERSION)
    rows = {r['game']: r for r in catalog['games']}
    grouped = [game for _, _, games in GROUPS for game in games]
    if len(grouped) != len(set(grouped)) or not set(grouped).issubset(rows):
        raise ValueError('Invalid or overlapping grouping plan')
    for row in rows.values():
        archive = source / row['archive']
        if archive.parent != source or archive.stat().st_size != row['bytes'] or sha(archive) != row['sha256']:
            raise ValueError('Source archive differs from catalog: ' + row['archive'])
    output.mkdir(parents=True)
    packages = []
    for slug, title, games in GROUPS:
        archive = output / f'SimCopterRemake-{VERSION}-Music-{slug}.zip'
        entries, common, expected = [], {}, set()
        with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=1) as target:
            for game in games:
                with zipfile.ZipFile(source / rows[game]['archive']) as original:
                    manifest = json.loads(original.read('music-package.json'))
                    if manifest['schema'] != 1 or manifest['games'] != [game] or manifest['minimum_game_version'] != '1.0.14':
                        raise ValueError('Unexpected source package manifest')
                    for name in COMMON:
                        data = original.read(name)
                        if name in common and common[name] != data:
                            raise ValueError('Different installer/readme versions in source packages')
                        common[name] = data
                    source_names = set(COMMON + ['music-package.json'])
                    for entry in manifest['files']:
                        path = PurePosixPath(entry['path'])
                        if path.is_absolute() or '..' in path.parts or path.parts[0] != game or '\\' in entry['path']:
                            raise ValueError('Invalid source manifest path')
                        name = (PAYLOAD / entry['path']).as_posix()
                        if name in expected:
                            raise ValueError('Duplicate payload path')
                        data = original.read(name)
                        if len(data) != entry['bytes'] or hashlib.sha256(data).hexdigest() != entry['sha256']:
                            raise ValueError('Source payload checksum mismatch')
                        target.writestr(original.getinfo(name), data, compress_type=zipfile.ZIP_DEFLATED, compresslevel=1)
                        expected.add(name)
                        source_names.add(name)
                        entries.append(entry)
                    if len(original.namelist()) != len(source_names) or set(original.namelist()) != source_names:
                        raise ValueError('Unexpected file in original game ZIP')
            package = dict(schema=1, minimum_game_version='1.0.14', title=title, games=games, files=entries)
            target.writestr('music-package.json', json.dumps(package, indent=2, ensure_ascii=False).encode('utf-8'))
            for name in COMMON:
                data = common[name]
                if name == 'README-MUSIC.txt':
                    heading = 'MUSIC PACK: ' + title + '\n\nIncluded game collections:\n'
                    heading += ''.join('- ' + game + '\n' for game in games)
                    heading += '\nEach game remains a separate Radio collection after installation.\n\n'
                    data = heading.encode('utf-8') + data.replace(b'Download individual games OR', b'Download game packs OR')
                target.writestr(name, data)
        with zipfile.ZipFile(archive) as check:
            assert set(check.namelist()) == expected | set(COMMON + ['music-package.json'])
            for entry in entries:
                assert hashlib.sha256(check.read((PAYLOAD / entry['path']).as_posix())).hexdigest() == entry['sha256']
        result = dict(title=title, games=games, tracks=sum(rows[g]['tracks'] for g in games),
                      archive=archive.name, bytes=archive.stat().st_size, sha256=sha(archive))
        packages.append(result)
        print(f"Verified {title}: {result['bytes'] / 1_000_000:.1f} MB, {result['tracks']} recordings", flush=True)
    for game, row in rows.items():
        if game not in grouped:
            packages.append(dict(title=game, games=[game], **{k: row[k] for k in ('tracks', 'archive', 'bytes', 'sha256')}))
    packages.sort(key=lambda p: p['title'].casefold())
    game_rows = []
    for row in catalog['games']:
        package = next(p for p in packages if row['game'] in p['games'])
        game_rows.append(dict(row, package=package['title'], **{k: package[k] for k in ('archive', 'bytes', 'sha256')}))
    result = dict(catalog, schema=2, packages=packages, games=game_rows)
    assert len(game_rows) == len(rows) and sum(p['tracks'] for p in packages) == sum(r['tracks'] for r in rows.values())
    (output / 'Music-Downloads.json').write_text(json.dumps(result, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    print(f'Catalog: {len(game_rows)} game collections in {len(packages)} downloads; source archives preserved.', flush=True)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    regroup(args.source, args.output)
