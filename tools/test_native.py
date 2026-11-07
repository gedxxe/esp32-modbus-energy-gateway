"""Compile and exercise production C++ sources, then upstream's independent fixture check."""
from pathlib import Path
import json,subprocess,os,hmac,hashlib,random,sys
root=Path(__file__).resolve().parent.parent
os.chdir(root)
build=root/'.build';build.mkdir(exist_ok=True)
sanitize = '--sanitizers' in sys.argv
binary=build/('gateway-tests-ubsan' if sanitize else
              ('gateway-tests.exe' if os.name=='nt' else 'gateway-tests'))
sources=sorted((root/'firmware/src/core').glob('*.cpp'))+sorted((root/'firmware/src/storage').glob('*.cpp'))
cmd=['g++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Ifirmware/include','-Ifirmware/include/app','-Ifirmware/include/core','-Ifirmware/include/storage','-Ifirmware/include/web','tests/native.cpp',*[str(p) for p in sources],'-o',str(binary)]
if sanitize:
    # The quality gate must detect undefined C++ behavior (not only compile).
    # Host-only instrumentation: never included in ESP32-C3 production firmware.
    cmd.extend(['-fsanitize=undefined', '-fno-sanitize-recover=all',
                '-fno-omit-frame-pointer', '-g'])
print('Compiling production core with native g++' +
      (' (UBSan)' if sanitize else ''), flush=True)
subprocess.run(cmd,check=True)
subprocess.run([str(binary)],check=True)
v=json.loads((root/'docs/lora-v1-vector.json').read_text())
subprocess.run([str(binary),'--vector',v['key_hex'],v['packet_hex']],check=True)
subprocess.run([os.sys.executable,str(root/'tools/check_lora.py')],check=True)
rng=random.Random(0xc3)
lengths=[0,1,55,56,63,64,65,111,112,119,120,127,128,135,136]+[rng.randrange(137) for _ in range(100)]
for n in lengths:
    key=bytes(rng.randrange(256) for _ in range(32)); data=bytes(rng.randrange(256) for _ in range(n))
    got=subprocess.check_output([str(binary),'--hmac',key.hex(),data.hex()],text=True)
    assert got==hmac.new(key,data,hashlib.sha256).hexdigest()
print(f'PASS: {len(lengths)} production HMAC results cross-checked with Python SHA256 (padding/block boundaries included)')
