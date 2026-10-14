"""Stage reviewed optional recordings without changing their encoded audio bytes.

Input is the local audit directory containing comparison.json and its source files.
FFmpeg/ffprobe are validation dependencies only; they never encode the supplied music.
"""
import argparse
import concurrent.futures
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(4 * 1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def inspect(path):
    probe = subprocess.run(['ffprobe', '-v', 'error', '-select_streams', 'a:0',
        '-show_streams', '-show_format', '-of', 'json', str(path)], capture_output=True, check=True)
    info = json.loads(probe.stdout)
    stream = info['streams'][0]
    # Decode independently to check the entire recording, duration and PCM identity.
    process = subprocess.Popen(['ffmpeg', '-v', 'error', '-i', str(path), '-map', '0:a:0',
        '-f', 's16le', '-'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    digest = hashlib.md5()
    size = 0
    while True:
        block = process.stdout.read(1024 * 1024)
        if not block:
            break
        digest.update(block)
        size += len(block)
    errors = process.stderr.read().decode('utf-8', errors='replace').strip()
    if process.wait() != 0 or not size:
        raise RuntimeError(f'Audio decode failed: {path}: {errors}')
    channels, rate = int(stream['channels']), int(stream['sample_rate'])
    if channels not in (1, 2) or size % (2 * channels):
        raise ValueError(f'Unsupported audio format: {path}')
    return {'channels': channels, 'sample_rate': rate, 'frames': size // (2 * channels),
        'pcm_md5': digest.hexdigest(), 'bits_per_raw_sample': stream.get('bits_per_raw_sample'),
        'tags': info.get('format', {}).get('tags', {}), 'decode_notes': errors}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('audit', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    audit = args.audit.resolve()
    doc = json.loads((audit / 'comparison.json').read_text(encoding='utf-8'))
    selected, excluded = [], list(doc['excluded_nonmusic'])
    for row in doc['candidates']:
        row = dict(row)
        if row.get('excluded_reason'):
            row['reason'] = row['excluded_reason']
            excluded.append(row)
        elif row['source'] == 'simcopter' or row['game'] == 'SimCopter':
            if row['correlation'] < .8:
                raise ValueError('SimCopter match needs manual review: ' + row['path'])
            row['reason'] = 'Same recording already supplied in SimCopter (including existing FLAC replacements)'
            excluded.append(row)
        elif 'sound effect' in row['path'].lower():
            row['reason'] = 'Weather/disaster effect, not music'
            excluded.append(row)
        else:
            selected.append(row)
    selected.sort(key=lambda r: (r['game'], r['variant'], r['path'].lower()))
    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
        details = list(pool.map(lambda row: inspect(audit / row['path']), selected))
    games, seen = {}, {}
    for row, info in zip(selected, details):
        source = (audit / row['path']).resolve()
        if not source.is_relative_to(audit) or sha(source) != row['source_sha256']:
            raise ValueError('Source changed: ' + str(source))
        # Identical decoded audio with different metadata is one song within a game.
        identity = (row['game'], info['channels'], info['sample_rate'], info['pcm_md5'])
        if identity in seen:
            row['reason'] = 'Identical decoded recording already in this game collection: ' + seen[identity]
            excluded.append(row)
            continue
        seen[identity] = row['path']
        tracks = games.setdefault(row['game'], [])
        title = row.get('title') or re.sub(r'^\d+[ ._-]+', '', Path(row.get('original_path', row['path'])).stem)
        title = re.sub(r'[<>:"/\\|?*\x00-\x1f]', '-', title).strip(' .')
        variant = row['variant']
        if row['source'] == 'simeverything': variant += ' - SimEverything'
        filename = f'{len(tracks)+1:03d} {variant} - {title}'[:170] + source.suffix.lower()
        target = args.output / row['game'] / filename
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        if sha(target) != row['source_sha256']:
            raise RuntimeError('Staged bytes differ: ' + filename)
        tracks.append(dict(row, **info, file=filename, bytes=source.stat().st_size,
            sha256=row['source_sha256'], seconds=info['frames']/info['sample_rate']))
    for game, tracks in games.items():
        folder = args.output / game
        manifest = {'schema': 1, 'game': game, 'minimum_game_version': '1.0.14',
            'source_collection': 'https://community.simtropolis.com/forums/topic/48504-comprehensive-simcity-music-collection-version-32/',
            'audio_policy': 'No lossy conversion, resampling or channel reduction. Supplied audio copied byte-for-byte except four Ogg container repairs with identical decoded PCM; untouched originals retained in Source Notes.',
            'additional_music_reference': 'https://archive.org/details/SimEverything',
            'additional_music_reference_note': 'User-supplied SimEverything archive included in the reviewed music collection; an untouched preservation download is available separately.',
            'tracks': tracks}
        (folder / 'collection.json').write_text(json.dumps(manifest, indent=2, ensure_ascii=False)+'\n', encoding='utf-8')
        credits = folder / 'Source Notes'
        for row in tracks:
            if row.get('original_path'):
                original = audit / row['original_path']
                if sha(original) != row['original_sha256']: raise ValueError('Original source changed')
                originals = credits / 'Original containers'
                originals.mkdir(parents=True, exist_ok=True)
                shutil.copy2(original, originals / (row['source'] + ' - ' + original.name))
        for key in {r['source'] for r in tracks}:
            for note in (audit / 'extracted' / key).rglob('*.txt'):
                credits.mkdir(exist_ok=True)
                shutil.copy2(note, credits / (key+' - '+note.name))
        if game == 'SimCity':
            for midi in (audit / 'extracted/simcity-windows').glob('*.[mM][iI][dD]'):
                (credits / 'Original MIDI').mkdir(parents=True, exist_ok=True)
                shutil.copy2(midi, credits / 'Original MIDI' / midi.name)
    summary = {'games': {game: {'tracks': len(tracks), 'bytes': sum(r['bytes'] for r in tracks),
        'seconds': sum(r['seconds'] for r in tracks)} for game, tracks in games.items()}, 'excluded': excluded}
    (audit / 'prepared.json').write_text(json.dumps(summary, indent=2, ensure_ascii=False)+'\n', encoding='utf-8')
    print(json.dumps(summary['games'], indent=2), flush=True)
    print('Total tracks:', sum(len(t) for t in games.values()), flush=True)


if __name__ == '__main__':
    main()
