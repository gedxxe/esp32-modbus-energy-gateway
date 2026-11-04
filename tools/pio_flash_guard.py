"""Pinned pioarduino 55.03.311 factory-partition upload guard.

The platform parses only ota_0 for app offset and unconditionally adds boot_app0
at 0xE000. This project has a factory app at 0x20000 and gateway NVS at 0xE000.
Fix the generated/upload arguments locally, never patch the installed platform.
"""
Import("env")  # SCons-provided build environment
from pathlib import Path
import json

def enforce(source=None, target=None, env=env):
    env.Replace(ESP32_APP_OFFSET="0x20000")
    env.BoardConfig().update("upload.maximum_size", 0x200000)
    env["INTEGRATION_EXTRA_DATA"]["application_offset"] = "0x20000"
    images=[x for x in env.get("FLASH_EXTRA_IMAGES",[]) if not str(x[1]).replace('\\','/').endswith('/boot_app0.bin')]
    env.Replace(FLASH_EXTRA_IMAGES=images)
    flags=list(env.get("UPLOADERFLAGS",[]))
    cleaned=[]; i=0
    while i<len(flags):
        if i+1<len(flags) and str(flags[i+1]).strip('"').replace('\\','/').endswith('/boot_app0.bin'):
            i+=2
        else:
            cleaned.append(flags[i]);i+=1
    env.Replace(UPLOADERFLAGS=cleaned)
    assert not any('boot_app0.bin' in str(x) for x in cleaned)
    for offset,path in images:
        start=int(str(offset),0)
        assert start in (0,0x8000),f'Unexpected extra flash image at {offset}'
    build=Path(env.subst('$BUILD_DIR'));build.mkdir(parents=True,exist_ok=True)
    (build/'flash-layout.json').write_text(json.dumps({'chip':'esp32c3','app_offset':0x20000,'app_capacity':0x200000,'extra_images':[{'offset':int(str(o),0),'name':Path(str(p)).name} for o,p in images],'boot_app0_removed':True,'gateway_nvs_offset':0xe000,'archives_offset':0x220000},indent=2)+'\n',encoding='utf-8')

enforce()
env.AddPreAction('checkprogsize',enforce)
env.AddPreAction('$BUILD_DIR/${PROGNAME}.bin',enforce)
env.AddPreAction('upload',enforce)
print('Gateway flash guard: factory app @ 0x20000, 2 MiB; boot_app0 removed; gw_nvs preserved')
