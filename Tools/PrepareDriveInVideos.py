"""Prepare aspect-preserving 1080p H.264/AAC drive-in movies; keep source files intact."""
import argparse
import json
import subprocess
import tempfile
from pathlib import Path

def probe(path):
    return json.loads(subprocess.check_output(['ffprobe', '-v', 'error', '-show_streams', '-show_format', '-of', 'json', str(path)]))

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('inputs', type=Path, nargs='+')
    ap.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[1] / 'SimCopterRemake/Content/DriveInVideos')
    ap.add_argument('--crf', default=23, type=int)
    ap.add_argument('--max-video-rate', default='2800k')
    ap.add_argument('--target-mb', type=float, help='Two-pass size target in decimal MB (uses 720p HD)')
    args = ap.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    results = []
    for source in args.inputs:
        info = probe(source)
        original = next(s for s in info['streams'] if s['codec_type'] == 'video')
        target = args.output / source.name
        if source.resolve() == target.resolve():
            raise ValueError('Output must differ from source')
        temporary = target.with_suffix('.tmp.mp4')
        # Lanczos and modest sharpening avoid fabricated content, stretching, and cropping.
        height = 720 if args.target_mb else 1080
        vf = f'scale=-2:{height}:flags=lanczos,setsar=1'
        if original['height'] < height:
            vf += ',unsharp=5:5:0.35:5:5:0'
        base = ['ffmpeg', '-hide_banner', '-loglevel', 'warning', '-y', '-i', str(source),
            '-map', '0:v:0', '-vf', vf, '-c:v', 'libx264', '-preset', 'slow', '-pix_fmt', 'yuv420p']
        if args.target_mb:
            # Reserve mux overhead and 96 kbps stereo audio; preserve the source frame rate.
            bitrate = int(args.target_mb * 1_000_000 * 8 * .975 / float(info['format']['duration']) - 96000)
            if bitrate < 100000: raise ValueError('Target too small for HD video')
            with tempfile.TemporaryDirectory(prefix='.encoding-', dir=args.output) as work:
                rate = ['-b:v', str(bitrate), '-passlogfile', str(Path(work) / 'pass')]
                subprocess.run(base + rate + ['-pass', '1', '-an', '-f', 'null', '-'], check=True)
                subprocess.run(base + rate + ['-pass', '2', '-map', '0:a:0?', '-c:a', 'aac', '-b:a', '96k', '-movflags', '+faststart', str(temporary)], check=True)
        else:
            subprocess.run(base + ['-map', '0:a:0?', '-crf', str(args.crf), '-maxrate', args.max_video_rate, '-bufsize', '5600k',
                '-c:a', 'aac', '-b:a', '160k', '-movflags', '+faststart', str(temporary)], check=True)
        result = probe(temporary)
        video = next(s for s in result['streams'] if s['codec_type'] == 'video')
        assert video['height'] == height
        assert video['r_frame_rate'] == original['r_frame_rate']
        assert abs(float(result['format']['duration']) - float(info['format']['duration'])) < .2
        assert any(s['codec_type'] == 'audio' for s in result['streams']) == any(s['codec_type'] == 'audio' for s in info['streams'])
        temporary.replace(target)
        results.append({'source': str(source), 'source_bytes': source.stat().st_size, 'output_bytes': target.stat().st_size, 'source_size': [original['width'], original['height']], 'output': str(target), 'size': [video['width'], video['height']], 'frame_rate': video['r_frame_rate'], 'duration': result['format']['duration']})
        print(json.dumps(results[-1]), flush=True)
    manifest = args.output / 'prepared-videos.json'
    previous = json.loads(manifest.read_text(encoding='utf-8')) if manifest.exists() else []
    results += [entry for entry in previous if Path(entry['output']).name not in {p.name for p in args.inputs}]
    manifest.write_text(json.dumps(results, indent=2), encoding='utf-8')

if __name__ == '__main__':
    main()
