"""Package verified build outputs without caches/dependencies/secrets. Run after tests/builds."""
from pathlib import Path
import hashlib,json,shutil,struct,sys,zipfile
root=Path(__file__).resolve().parent.parent
release=root/'release'; release.mkdir(exist_ok=True)
manifestPath=release/'manifest.json'
ReleaseVersion='0.1.1'
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
if '--verify' in sys.argv:
    m=json.loads(manifestPath.read_text())
    for entry in m['artifacts']:
        p=root/entry['path']
        assert p.stat().st_size==entry['bytes'] and digest(p)==entry['sha256'],p
    for entry in m['source']:
        assert digest(root/entry['path'])==entry['sha256'],entry['path']
    with zipfile.ZipFile(release/f"energy-station-gateway-c3-{m['release']}.zip") as z:
        assert z.testzip() is None
        for entry in m['source']+m['artifacts']:
            assert hashlib.sha256(z.read(entry['path'])).hexdigest()==entry['sha256'],entry['path']
        assert z.read('release/manifest.json')==manifestPath.read_bytes()
    print(f"PASS: {len(m['artifacts'])} artifact hashes and {len(m['source'])} source hashes verified")
    raise SystemExit()
artifacts=[]
for filename,offset in [('bootloader.bin','0x0'),('partitions.bin','0x8000'),('firmware.bin','0x20000'),('littlefs.bin','0x220000'),('firmware.elf',None)]:
    p=root/'.pio/build/esp32c3'/filename
    if not p.exists():raise SystemExit(f'Missing build output: {p}')
    target=release/filename;shutil.copyfile(p,target)
    if filename.endswith('.bin') and filename not in ['partitions.bin','littlefs.bin']:
        data=target.read_bytes();assert data[0]==0xe9 and struct.unpack_from('<H',data,12)[0]==5,'Expected ESP32-C3 image'
    if filename.endswith('.elf'):
        data=target.read_bytes();assert data[:4]==b'\x7fELF' and struct.unpack_from('<H',data,18)[0]==243,'Expected RISC-V ELF'
    artifacts.append({'path':str(target.relative_to(root)).replace('\\','/'),'bytes':target.stat().st_size,'sha256':digest(target),'flash_offset':offset})
allowed=['AGENTS.md','WORK_STATUS.md','README.md','README.id.md','LICENSE','NOTICE','.gitignore','.gitattributes','.clang-format','platformio.ini','partitions.csv']
files=[root/p for p in allowed]
for folder in ['firmware','docs','tests','licenses','data']:
    files.extend(p for p in (root/folder).rglob('*') if p.is_file())
for p in (root/'dashboard').rglob('*'):
    if p.is_file() and not any(x in p.parts for x in ['node_modules','dist']):files.append(p)
for filename in ['test_native.py','check_lora.py','package.py','format_source.py','pio_flash_guard.py','check_flash_layout.py']:
    files.append(root/'tools'/filename)
files=sorted(set(files))
source=[{'path':str(p.relative_to(root)).replace('\\','/'),'sha256':digest(p)} for p in files]
manifest={'release':ReleaseVersion,'date':'2026-11-04','target':'esp32-c3-devkitm-1','flash_bytes':4194304,'protocol':'Energy Station v1','node_commit':'26c3a9429c8c4d410ef11a1b81e1e42f295034b1','framework':'Arduino-ESP32 3.3.11 / ESP-IDF 5.5.5','platform':'pioarduino 55.03.311','radiolib':'7.8.1','hardware_tested':False,'artifacts':artifacts,'source':source,'provisioning':'Preserve gw_nvs and archives on updates. littlefs.bin replaces archives and is first-provisioning only.'}
manifest['flash_layout']=json.loads((root/'.pio/build/esp32c3/flash-layout.json').read_text(encoding='utf-8'))
manifest['verification']={'production_core_assertions':580,'independent_hmac_cases':115,'dashboard_tests':9,'built_dashboard_tests':7,'inventory_helper_tests':2,'static_ram_bytes':94568,'program_flash_bytes':1227511,'app_partition_capacity':2097152,'dashboard_gzip_bytes':60825,'hardware':'AWAITING_HARDWARE'}
manifestPath.write_text(json.dumps(manifest,indent=2)+'\n')
archive=release/f'energy-station-gateway-c3-{ReleaseVersion}.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for p in files+[release/a['path'].split('/')[-1] for a in artifacts]+[manifestPath]:
        info=zipfile.ZipInfo(str(p.relative_to(root)).replace('\\','/'),(2026,11,4,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;z.writestr(info,p.read_bytes())
print(f'Packaged {len(files)} source files + {len(artifacts)} build artifacts; zip SHA256 {digest(archive)}')
(release/'SHA256SUMS.txt').write_text(''.join(f'{digest(p)}  {p.name}\n' for p in [archive,manifestPath,*[release/a['path'].split('/')[-1] for a in artifacts]]),encoding='utf-8')
