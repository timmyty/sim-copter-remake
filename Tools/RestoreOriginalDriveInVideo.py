#!/usr/bin/env python3
"""Restore the supplied 128x128 original movie (32x32 picture plus fixed black padding).

Requires ffmpeg/ffprobe, numpy, Pillow, and the official Real-ESRGAN ncnn Vulkan
portable tool. These are preparation tools only; the game just plays the MP4.
https://github.com/xinntao/Real-ESRGAN/releases/tag/v0.2.5.0
"""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image


def run(command, log=None):
    subprocess.run([str(arg) for arg in command], check=True, stdout=log, stderr=log)


def probe(path):
    return json.loads(subprocess.check_output([
        'ffprobe', '-v', 'error', '-show_streams', '-show_format', '-of', 'json', str(path)]))


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--upscaler', type=Path, required=True, help='realesrgan-ncnn-vulkan.exe')
    parser.add_argument('--work', type=Path, required=True, help='Fresh evidence/intermediate directory')
    parser.add_argument('--height', type=int, default=1440)
    parser.add_argument('--strength', type=float, default=.6, help='Neural restoration blend, 0..1')
    args = parser.parse_args()
    if args.source.resolve() == args.output.resolve():
        parser.error('The original source must not be overwritten')
    if args.height < 720 or args.height % 2 or not 0 <= args.strength <= 1:
        parser.error('Use an even HD height and strength between 0 and 1')
    info = probe(args.source)
    video = next(s for s in info['streams'] if s['codec_type'] == 'video')
    if (video['width'], video['height']) != (128, 128):
        parser.error('This restoration is for the verified 128x128 original with centered 32x32 picture')
    args.work.mkdir(parents=True, exist_ok=False)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    inputs, restored, blended = [args.work / name for name in ('input', 'restored', 'blended')]
    for folder in (inputs, restored, blended):
        folder.mkdir()
    source_hash = sha256(args.source)

    # Inspect EVERY frame, not just the opening image. Reject a different movie
    # whose actual content would be lost. Allow only low-level codec ringing in
    # the border around the known 32x32 authored movie window.
    raw = subprocess.check_output(['ffmpeg', '-v', 'error', '-i', str(args.source),
                                  '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'])
    frames = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 128, 128, 3)
    border = np.ones((128, 128), dtype=bool)
    border[48:80, 48:80] = False
    outside = frames[:, border, :]
    border_peak = int(outside.max())
    border_bright_fraction = float((outside.max(axis=2) > 16).mean())
    if border_peak > 40 or border_bright_fraction > .0001:
        raise ValueError('Content outside the expected picture window; refusing automatic crop')
    del raw, frames, outside
    run(['ffmpeg', '-v', 'error', '-y', '-i', args.source, '-vf', 'crop=32:32:48:48',
         '-fps_mode', 'passthrough', inputs / 'frame%08d.png'])
    with (args.work / 'restoration.log').open('w') as log:
        run([args.upscaler, '-i', inputs, '-o', restored, '-m', args.upscaler.parent / 'models',
             '-n', 'realesrgan-x4plus', '-s', '4', '-t', '128', '-j', '1:1:1', '-f', 'png'], log)
    source_frames = sorted(inputs.glob('frame*.png'))
    if len(source_frames) != len(list(restored.glob('frame*.png'))):
        raise ValueError('Restoration did not return every frame')
    for frame in source_frames:
        with Image.open(frame) as src, Image.open(restored / frame.name) as ai:
            original = src.convert('RGB').resize((128, 128), Image.Resampling.LANCZOS)
            if ai.size != (128, 128):
                raise ValueError(f'Unexpected restored dimensions: {ai.size}')
            Image.blend(original, ai.convert('RGB'), args.strength).save(blended / frame.name)

    temporary = args.output.with_suffix('.restoring.mp4')
    with (args.work / 'encoding.log').open('w') as log:
        # Light temporal cleanup limits frame-to-frame crawl. Keep the original
        # cadence: no invented intermediate frames or second neural upscale pass.
        run(['ffmpeg', '-hide_banner', '-y', '-framerate', video['r_frame_rate'],
             '-i', blended / 'frame%08d.png', '-i', args.source, '-map', '0:v:0', '-map', '1:a:0?',
             '-vf', f'hqdn3d=0.5:0.5:1:1,scale={args.height}:{args.height}:flags=lanczos,setsar=1',
             '-c:v', 'libx264', '-preset', 'slow', '-crf', '16', '-pix_fmt', 'yuv420p',
             '-c:a', 'copy', '-movflags', '+faststart', temporary], log)
    result = probe(temporary)
    output_video = next(s for s in result['streams'] if s['codec_type'] == 'video')
    assert (output_video['width'], output_video['height']) == (args.height, args.height)
    assert output_video['r_frame_rate'] == video['r_frame_rate']
    assert int(output_video['nb_frames']) == len(source_frames)
    assert abs(float(result['format']['duration']) - float(info['format']['duration'])) < .1
    assert sha256(args.source) == source_hash
    temporary.replace(args.output)
    manifest = {
        'source_name': args.source.name, 'source_sha256': source_hash,
        'source_size': [128, 128], 'active_picture': [48, 48, 32, 32],
        'border_peak': border_peak, 'border_bright_fraction': border_bright_fraction,
        'output_name': args.output.name, 'output_sha256': sha256(args.output),
        'output_bytes': args.output.stat().st_size, 'size': [args.height, args.height],
        'frame_rate': output_video['r_frame_rate'], 'frames': len(source_frames),
        'duration': result['format']['duration'], 'model': 'realesrgan-x4plus',
        'model_sha256': sha256(args.upscaler.parent / 'models/realesrgan-x4plus.bin'),
        'neural_strength': args.strength, 'projection_linear_increase': 4,
        'note': 'Restoration of very low-resolution footage; not native HD detail. Audio copied unchanged.'
    }
    (args.work / 'restoration.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    print(json.dumps(manifest), flush=True)


if __name__ == '__main__':
    main()
