"""Validate actual build layout, image placement and absence of fixture credentials."""
from pathlib import Path
import json,struct
root=Path(__file__).resolve().parent.parent
build=root/'firmware/.pio/build/esp32c3'
layout=json.loads((build/'flash-layout.json').read_text(encoding='utf-8'))
assert layout['app_offset']==0x20000 and layout['app_capacity']==0x200000
assert layout['boot_app0_removed']
assert {x['offset'] for x in layout['extra_images']}=={0,0x8000}
parts={}
data=(build/'partitions.bin').read_bytes()
for i in range(0,len(data),32):
    if struct.unpack_from('<H',data,i)[0]!=0x50aa:break
    _,kind,subtype,offset,size,name,flags=struct.unpack_from('<HBBII16sI',data,i)
    parts[name.split(b'\0')[0].decode()]=(kind,subtype,offset,size)
assert parts['factory']==(0,0,0x20000,0x200000)
assert parts['gw_nvs']==(1,2,0xe000,0xb000)
assert parts['archives'][2:]==(0x220000,0x1e0000)
app=(build/'firmware.bin').read_bytes(); combined=(build/'firmware.factory.bin').read_bytes()
assert len(app)<=0x200000
assert combined[0x20000:0x20000+len(app)]==app
assert set(combined[0xe000:0x19000])=={255},'Unexpected factory content over gateway NVS'
assert len((build/'littlefs.bin').read_bytes())==0x1e0000
fixture=json.loads((root/'docs/lora-v1-vector.json').read_text(encoding='utf-8'))
assert fixture['key_hex'].encode() not in app
assert bytes.fromhex(fixture['packet_hex']) not in app
print('PASS: actual factory partition, app @0x20000, no boot_app0 overwrite, 4 MB layout and filesystem capacity; no fixture packet/key text')
