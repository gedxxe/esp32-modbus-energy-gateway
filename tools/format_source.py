from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parent.parent
formatter=root/'.build/pydeps/clang_format/data/bin/clang-format.exe'
if not formatter.exists():
    formatter=Path('clang-format')
files=[p for folder in ['firmware','tests'] for p in (root/folder).rglob('*') if p.suffix in ['.h','.cpp'] and p.name!='assets.h']
subprocess.run([str(formatter),'-i',*[str(p) for p in files]],check=True)
