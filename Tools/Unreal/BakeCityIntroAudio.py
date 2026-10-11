#!/usr/bin/env python3
"""Decode the user's CITY0_B..CITY29_B.SMK soundtracks without resampling or remixing."""
import argparse
import hashlib
import json
import struct
import subprocess
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT / 'Reference/SimCopterOriginalGame/smk')
    parser.add_argument('--output', type=Path, default=ROOT / 'Reference/SimCopterOriginalGame/sound/cityride')
    args = parser.parse_args()
    sources = {p.name.lower(): p for p in args.source.glob('*') if p.is_file()}
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = []
    for city in range(30):
        name = f'CITY{city}_B.SMK'
        source = sources.get(name.lower())
        if source is None:
            raise SystemExit(f'Missing {name} in {args.source}; use the full cityride movies from your CD, not CITY*_S previews.')
        header = source.read_bytes()[:20]
        if header[:4] != b'SMK2' or struct.unpack_from('<4i', header, 4) != (400, 216, 451, -6666):
            raise SystemExit(f'Unexpected original cityride header: {source}')
        output = args.output / f'city{city}.wav'
        temporary = output.with_suffix('.tmp.wav')
        try:
            subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', str(source),
                            '-map', '0:a:0', '-c:a', 'pcm_s16le', '-map_metadata', '-1',
                            str(temporary)], check=True)
            with wave.open(str(temporary)) as audio:
                if (audio.getframerate(), audio.getnchannels(), audio.getsampwidth(), audio.getnframes()) != (11025, 2, 2, 331450):
                    raise SystemExit(f'Unexpected original audio format/duration: {source}')
                pcm = audio.readframes(audio.getnframes())
            manifest.append(dict(city=city, source=name, file=output.name,
                                 sample_rate=11025, channels=2, frames=331450,
                                 pcm_sha256=hashlib.sha256(pcm).hexdigest()))
            temporary.replace(output)
        finally:
            temporary.unlink(missing_ok=True)
    (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(f'Verified {len(manifest)} city soundtracks ({len({r["pcm_sha256"] for r in manifest})} distinct recordings), 30.063492 seconds each: {args.output}')


if __name__ == '__main__':
    main()
