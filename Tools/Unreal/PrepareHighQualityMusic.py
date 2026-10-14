"""Install only the audited lossless masters; keep reference WAVs for fallback and verification."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(4 * 1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('soundtrack', type=Path, help='Folder containing the 15 supplied cptr_*.flac files')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    sound = root / 'Reference/SimCopterOriginalGame/sound'
    manifest = json.loads((root / 'SimCopterRemake/Config/HighQualityMusic.json').read_text())
    files = []
    for track in manifest['tracks']:
        relative = Path(track['file'])
        assert not relative.is_absolute() and '..' not in relative.parts and relative.parts[0] == 'music-hq'
        source = args.soundtrack / relative.name
        assert source.is_file() and sha(source) == track['sha256'], f'Unverified replacement: {source}'
        for original in track['originals']:
            path = sound / original['path']
            assert path.is_file() and sha(path) == original['sha256'], f'Original changed or absent: {path}'
        target = sound / relative
        assert not target.exists() or sha(target) == track['sha256'], f'Destination already contains different music: {target}'
        files.append((source, target, track['sha256']))
    for source, target, digest in files:
        target.parent.mkdir(parents=True, exist_ok=True)
        if not target.exists():
            shutil.copy2(source, target)
        assert sha(target) == digest, target
    print(f'Verified {len(files)} shared FLAC files ({sum(p.stat().st_size for _, p, _ in files):,} bytes). Reference WAVs retained; game staging omits matched originals.')


if __name__ == '__main__':
    main()
