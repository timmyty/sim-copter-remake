"""Encode the 21 KINV masters as stereo Vorbis; retain masters outside staged Content."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import wave


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('masters', type=Path)
    parser.add_argument('--ffmpeg', default='ffmpeg')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    output = root / 'SimCopterRemake/Content/Radio/InvokeTheRevoked'
    tracks = json.loads((root / 'Docs/InvokeRadioTracks.json').read_text(encoding='utf-8'))
    assert len(tracks) == 21
    output.mkdir(parents=True, exist_ok=True)
    result = []
    for entry in tracks:
        source = args.masters / entry['output']
        original_hash = hashlib.sha256(source.read_bytes()).hexdigest()
        assert original_hash == entry['sha256'], f'Original master changed: {source.name}'
        target = output / (source.stem + '.ogg')
        subprocess.run([args.ffmpeg, '-v', 'error', '-y', '-i', str(source), '-map', '0:a:0',
            '-map_metadata', '-1', '-c:a', 'libvorbis', '-b:a', '128k', '-ar', '44100', '-ac', '2',
            str(target)], check=True)
        with wave.open(str(source)) as wav:
            frames = wav.getnframes()
        # Decode the complete file independently; reject missing tails and invalid packets.
        pcm = subprocess.check_output([args.ffmpeg, '-v', 'error', '-xerror', '-i', str(target),
            '-f', 's16le', '-acodec', 'pcm_s16le', '-'])
        assert abs(len(pcm) // 4 - frames) <= 1024, target.name
        result.append(dict(file=target.name, sha256=hashlib.sha256(target.read_bytes()).hexdigest(),
            bytes=target.stat().st_size, source_bytes=source.stat().st_size, source_sha256=original_hash,
            frames=frames, channels=2, sample_rate=44100))
        print(f'{target.name}: {target.stat().st_size / 1024**2:.2f} MiB', flush=True)
    manifest = dict(codec='Ogg Vorbis', nominal_bitrate=128000, tracks=result)
    (root / 'SimCopterRemake/Config/KinvMusic.json').write_text(json.dumps(manifest, indent=2)+'\n')
    print(json.dumps(dict(original_bytes=sum(t['source_bytes'] for t in result),
        encoded_bytes=sum(t['bytes'] for t in result))), flush=True)


if __name__ == '__main__':
    main()
