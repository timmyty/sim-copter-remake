"""Assemble a Shipping download and matching optional Development overlay from UAT staging."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(4 * 1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def inventory(root):
    return [{'path': p.relative_to(root).as_posix(), 'bytes': p.stat().st_size, 'sha256': sha(p)}
            for p in sorted(root.rglob('*')) if p.is_file()]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stage', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--engine', type=Path, default=Path('C:/Program Files/Epic Games/UE_5.8'))
    parser.add_argument('--version', required=True)
    parser.add_argument('--source-commit', required=True)
    parser.add_argument('--sevenzip', default='C:/Program Files/7-Zip/7z.exe')
    parser.add_argument('--archive', action='store_true', help='Archive previously assembled, smoke-tested folders')
    args = parser.parse_args()
    assert re.fullmatch(r'v\d+\.\d+\.\d+', args.version)
    assert re.fullmatch(r'[0-9a-f]{40}', args.source_commit)
    root = Path(__file__).resolve().parents[2]
    base = args.output / f'SimCopterRemake-{args.version}-Windows-x64'
    optional = args.output / 'Optional-Development'
    optional_name = f'SimCopterRemake-{args.version}-Optional-Development-Windows-x64.7z'
    developer = 'SimCopterRemake/Binaries/Win64/SimCopterRemake.exe'
    pix = 'Engine/Binaries/ThirdParty/Windows/WinPixEventRuntime/x64/WinPixEventRuntime.dll'
    launchers = ['Launch-With-GPU-Diagnostics.cmd', 'Launch-With-Performance-Diagnostics.cmd']
    if not args.archive:
        assert not base.exists() and not optional.exists(), 'Use a fresh destination'
        args.output.mkdir(parents=True, exist_ok=True)
        manifest = (args.stage / 'Manifest_NonUFSFiles_Win64.txt').read_text(encoding='utf-8-sig')
        for line in manifest.splitlines():
            rel = Path(line.split('\t')[0])
            assert not rel.is_absolute() and '..' not in rel.parts and (args.stage / rel).is_file(), rel
        for path in args.stage.rglob('*'):
            if not path.is_file():
                continue
            rel = path.relative_to(args.stage)
            if (path.suffix.lower() in {'.pdb', '.sav', '.dmp', '.log'} or
                    any(p.lower() in {'saved', 'logs', 'crashes', 'intermediate'} for p in rel.parts) or
                    rel.as_posix() in {developer, pix, *launchers, 'Manifest_DebugFiles_Win64.txt'}):
                continue
            target = base / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
        kinv = json.loads((root / 'SimCopterRemake/Config/KinvMusic.json').read_text())['tracks']
        music = base / 'SimCopterRemake/Content/Radio/InvokeTheRevoked'
        assert len(kinv) == 21 and {p.name for p in music.iterdir()} == {t['file'] for t in kinv}
        for track in kinv:
            assert sha(music / track['file']) == track['sha256']
        assert not (base / developer).exists()
        for directory in ['Docs']:
            for source in (root / directory).rglob('*'):
                rel = source.relative_to(root)
                if not source.is_file() or 'scratchpad' in rel.parts:
                    continue
                with source.open('rb') as stream:
                    assert not stream.read(64).startswith(b'version https://git-lfs.github.com/spec/v1'), f'Fetch LFS content before packaging: {rel}'
                target = base / rel
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, target)
        shutil.copy2(root / 'README.md', base / 'README.md')
        notices = root.parent / 'SimCopter Remake v1.0.1/Windows/NOTICES.txt'
        shutil.copy2(notices, base / 'NOTICES.txt')
        (base / 'START-HERE.txt').write_text(
            f'SimCopter Remake {args.version} - Windows x64\n\n'
            'Extract the complete game archive and run SimCopterRemake.exe.\n'
            'Keep all accompanying folders together. No Unreal Editor is needed.\n'
            'KINV contains all 21 songs in compact stereo Ogg Vorbis format.\n\n'
            'GPU/performance diagnostics are an optional separate download.\n'
            f'Download {optional_name} only if needed, and extract its CONTENTS\n'
            'into this game folder, beside this file and the normal launcher.\n'
            'Use the same version for both downloads.\n')
        for rel, source in [(developer, root / developer), (pix, args.engine / pix)] + [
                (name, root / 'Tools/Unreal' / name) for name in launchers]:
            target = optional / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
        (optional / 'README-OPTIONAL-DEVELOPMENT.txt').write_text(
            f'Optional Development diagnostics for SimCopter Remake {args.version}\n\n'
            f'Requires the complete {args.version} Windows x64 game download.\n'
            'Extract the CONTENTS of this archive into the extracted game folder,\n'
            'beside the root SimCopterRemake.exe. Merge the Engine and SimCopterRemake folders.\n'
            'This adds SimCopterRemake/Binaries/Win64/SimCopterRemake.exe; it does not replace\n'
            'the root launcher or the normal Shipping executable.\n\n'
            'Launch-With-GPU-Diagnostics.cmd collects GPU crash evidence.\n'
            'Launch-With-Performance-Diagnostics.cmd also records performance traces.\n'
            'Normal play continues to use the root SimCopterRemake.exe.\n\n'
            'Diagnostic text logs: %LOCALAPPDATA%/SimCopterRemake/Saved/Logs/\n'
            '  GPU-Diagnostics.log or Performance-Diagnostics.log, depending on the launcher.\n'
            'Crash reports and dumps, when generated: %LOCALAPPDATA%/SimCopterRemake/Saved/Crashes/\n'
            'Performance traces: %LOCALAPPDATA%/SimCopterRemake/Saved/Profiling/\n'
            'For a crash report, include the diagnostic log and the newest crash folder.\n')
        info = dict(version=args.version, source_commit=args.source_commit, kinv_codec='Ogg Vorbis 128 kbps stereo',
                    optional_development_archive=optional_name)
        (base / 'BUILD_INFO.json').write_text(json.dumps(info, indent=2)+'\n')
        (optional / 'DEVELOPMENT_BUILD_INFO.json').write_text(json.dumps(info, indent=2)+'\n')
        for label, folder in [('base', base), ('development', optional)]:
            entries = inventory(folder)
            (args.output / (label+'-inventory.json')).write_text(json.dumps(entries, indent=2)+'\n')
            print(label, len(entries), sum(e['bytes'] for e in entries), flush=True)
        return
    receipts = []
    for label, folder, filename in [('base', base, base.name+'.7z'), ('development', optional, optional_name)]:
        expected = json.loads((args.output / (label+'-inventory.json')).read_text())
        assert inventory(folder) == expected, f'{label} changed since assembly'
        archive = args.output / filename
        assert not archive.exists()
        # Main download has a named enclosing folder; optional download overlays its contents.
        cwd, item = (folder.parent, folder.name) if label == 'base' else (folder, '*')
        with (args.output / (label+'-archive.log')).open('w') as log:
            subprocess.run([args.sevenzip, 'a', '-t7z', '-mx=7', '-mmt=4', '-ms=256m', '-bsp0',
                            str(archive.resolve()), item], cwd=cwd, stdout=log, stderr=subprocess.STDOUT, check=True)
        verify = args.output / ('verify-'+label)
        assert not verify.exists()
        subprocess.run([args.sevenzip, 'x', str(archive), '-o'+str(verify), '-y', '-bsp0'], check=True,
                       stdout=subprocess.DEVNULL)
        unpacked = verify / folder.name if label == 'base' else verify
        assert inventory(unpacked) == expected, f'{label} archive contents mismatch'
        receipts.append(dict(archive=filename, bytes=archive.stat().st_size, sha256=sha(archive),
            files_verified=len(expected), uncompressed_bytes=sum(e['bytes'] for e in expected)))
        print(json.dumps(receipts[-1]), flush=True)
    (args.output / 'package-verified.json').write_text(json.dumps(dict(source_commit=args.source_commit,
        version=args.version, archives=receipts), indent=2)+'\n')
    (args.output / 'SHA256SUMS.txt').write_text(''.join(r['sha256']+'  '+r['archive']+'\n' for r in receipts))


if __name__ == '__main__':
    main()
