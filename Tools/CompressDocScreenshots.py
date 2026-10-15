"""Re-encode documentation screenshots at full resolution and repair Markdown links.

Original captures are retained in the ignored scratchpad, outside published Docs assets.
This does not touch in-game textures or artwork. Requires Pillow with WebP support.
"""
from pathlib import Path
import json
import os
import shutil
from PIL import Image


def main():
    root=Path(__file__).resolve().parents[1]
    folder=root/'Docs/screenshots'
    scratch=root/'Docs/scratchpad/civic-waterfront-2026-10-15'
    backup=scratch/'original-doc-screenshots'
    backup.mkdir(parents=True,exist_ok=True)
    rows=[]
    for source in sorted(folder.glob('*.png')):
        target=source.with_suffix('.webp')
        if target.exists(): raise RuntimeError(f'Refusing to replace {target}')
        with Image.open(source) as image:
            # Lossless for text-heavy UI guides; high-quality WebP for scene captures.
            lossless='cheat-' in source.name
            image.save(target,format='WEBP',quality=88,method=6,lossless=lossless,exact=True)
            with Image.open(target) as check:
                check.load()
                assert check.size==image.size
                if lossless: assert check.convert('RGBA').tobytes()==image.convert('RGBA').tobytes()
            before=source.stat().st_size; after=target.stat().st_size
            if after>=before:
                target.unlink()
                continue
            rows.append(dict(source=source.name,target=target.name,width=image.width,height=image.height,
                             before=before,after=after,lossless=lossless))
        shutil.copy2(source,backup/source.name)
    for base,dirs,files in os.walk(root):
        dirs[:]=[d for d in dirs if d not in ('.git','scratchpad','Binaries','Intermediate','Saved','Content','Reference','.ghidra-exports','.venv','node_modules')]
        for name in files:
            if not name.endswith('.md'): continue
            path=Path(base)/name
            original=path.read_bytes(); text=original.decode('utf-8-sig')
            for row in rows:
                text=text.replace('screenshots/'+row['source'],'screenshots/'+row['target'])
            if text!=original.decode('utf-8-sig'): path.write_bytes(text.encode('utf-8'))
    for row in rows: (folder/row['source']).unlink()
    report=dict(images=rows,before=sum(r['before'] for r in rows),after=sum(r['after'] for r in rows))
    (scratch/'screenshot-compression.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!='images'}),f'{len(rows)} screenshots, full resolution')


if __name__=='__main__': main()
