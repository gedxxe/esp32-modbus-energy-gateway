# Software and hardware verification

Date: **2026-11-04, Asia/Jakarta**. One owner: Codex. No physical ESP32/radio was flashed or tested. UI fixture data and native fault injection are explicitly software simulations.

## Executed commands

| Actual command / working directory | Result |
|---|---|
| `python tools/test_native.py` / repository root | PASS: **580** assertions on actual production core; unchanged public vector roundtrip; independent upstream Python check; **115** production HMAC cross-checks against Python SHA256 including block/padding boundaries |
| `npm install`, then `npm ci --no-audit --no-fund` / dashboard | Exact versions installed and final dependency lock replayed |
| `npm run build` / dashboard | PASS: actual HTML199,406 bytes, gzip60,825 bytes; generated firmware header, release0.1.1 |
| `npm test` / dashboard | PASS: **9** tests (7 actual-bundle +2 production inventory-helper tests): card isolation, add/duplicate-ID/verified-save flow, outdated form revisions, absent/stale data including stalled polling, asset byte identity, login/navigation/key hygiene/Indonesia toggle, unused-ID/capacity validation |
| `C:/Users/Lenovo/.platformio/penv/Scripts/platformio.exe run -e esp32c3` | Target build PASS; final size evidence below |
| Same executable `run -e esp32c3 -t buildfs` | PASS: first-provisioning LittleFS image,1,966,080 bytes |
| `python tools/check_flash_layout.py` | PASS: actual factory app0x20000,2 MiB capacity, no boot_app0 overlap, protected gw_nvs gap, filesystem size, no fixture packet/key text; blank inventory/key defaults tested in production C++ |
| `python tools/package.py` then `--verify` | Release0.1.1:5 build artifacts and81 source files; ZIP entries/CRC, SHA256 and embedded manifest verified; .gitattributes preserves bytes across Git clones |

Native sources are the same `firmware/core/*.cpp` and `firmware/storage/*.cpp` compiled into firmware, with no replacement codec. MinGW6.3 host execution is used because sandbox execution causes a Windows DLL relocation error. Production HMAC is also independently checked against Python's implementation. Host tests do not emulate electrical SPI, RF, flash brownout timing or the HTTP server's ESP-IDF runtime.

## Target/framework and memory

- Board: ESP32-C3-DevKitM-1 reference, 160 MHz, 4 MB physical flash, no PSRAM, one application CPU.
- pioarduino55.03.311; Arduino-ESP32 3.3.11; framework libs ESP-IDF5.5.5+sha.b774170ff46; RISC-V GCC14.2.0+20260121; RadioLib7.8.1; ArduinoJson7.3.1.
- Factory application capacity is2,097,152 bytes. Physical flash is4 MB. Platform's HARDWARE banner may say2 MB after app maximum is set; that represents the app limit. The image header/partition table are independently checked.
- Final0.1.1 build: **94,568 bytes static RAM /327,680 (28.9%)**; **1,227,511 bytes program flash /2,097,152 (58.5%)**. Per-node observability adds640 static RAM bytes over0.1.0. The16-frame replay window, storage-owner shared scratch and small serial summary remain. The radio stack is24 KiB, storage16 KiB, HTTP12 KiB, dynamically allocated and additional to static RAM.
- Dynamic task stacks, queues, Wi-Fi, HTTP JSON and filesystem consume additional heap. Radio/storage/HTTP minimum stack and free/minimum heap counters are exposed for bench verification. Compilation does not prove runtime headroom under16-node load.
- Pinned esptool5.3 image-info verifies **ESP32-C3 chip5**,4 MB flash,80 MHz,DIO image header, **valid checksum and validation hash**. Image file is1,267,696 bytes including segments/padding (program size above is the linker measure). Its prebuilt Arduino app-description reports framework metadata `arduino-lib-builder`/ESP-IDF5.5.5; gateway release identity0.1.1 is recorded separately in the dashboard/manifest, and protocol identity remainsv1.

## Fault and compatibility coverage

Three node IDs/keys; unknown/disabled/foreign source/session/correlation; HMAC and malformed packet rejection; exact duplicate/changed-sequence content handling; delayed duplicate ACKs; replay HELLO rate limits/current-state preservation; gateway-directed POLL rejection; monotonic reserved POLL sequence across reboot, corruption and full history; periodic/missing energy; interleaved same-node and other-node telemetry; out-of-order, duplicate, partial overlap, conflicting, corrupt and missing fragments; CRC/semantic rejection; one global archive; missing-day timeout; full ACK queue with transactional assembly preservation; ACK priority, coalescing, expiry and round robin; module-disabled/BUSY-timeout/30s-recovery/TX-timeout lifecycle; alternate-bank/interrupted writes; archive identity/idempotence and immutable content conflicts.

Driver lifecycle tests use the **same production lifecycle class**. RF and power-fail timings remain hardware work. V1 does not transmit remote node retry counters; dashboard does not invent those.

## Visual evidence and limits

Actual bundled React assets were loaded in the in-app browser. Phone and1280px desktop breakpoint DOM were checked; desktop root scroll width1265px within1280px. Mobile screenshot is saved in `assets/dashboard-mobile-qa.png`, with explicitly simulated fixture data. Native full-page capture was unavailable in the in-app browser, so no desktop screenshot is claimed. Radio state, power, pairing and archives in that fixture are not device evidence.

The0.1.1 card grid was separately checked with six simulated nodes plus Add node. At1280px the grid has three columns of approximately306.9px and no root overflow (1265px); mobile has one344px column. The Add node form proposed unused ID8 for existing IDs2–7. Received data expands to the correct per-node decoded fields. Screenshot: `assets/node-cards-mobile-qa.png`. Production C++ tests additionally prove inventory ID/name/key recovery after simulated reboot/interrupted save, and that duplicate authenticated packets update link-observation age without refreshing measurement age. Physical NVS/AP/RF tests remain pending.

Ra-01SH manufacturer manual pages11/12 were independently downloaded, hashed, rendered and visually reviewed. C3 board guide and installed board definition were inspected. Purchased board/module identity, wiring orientation, supply and physical RF switch behavior remain unverified.

## Failures resolved during software work

- Empty initial workspace; gateway repository initialized separately. Shell clone failed in sandbox DNS; all node audit reads used the read-only GitHub connector instead.
- First bundle assertion assumed English numeric separators; corrected to respect browser locale. A short-final-fragment test initially assumed400 covered bytes instead of346; corrected test arithmetic.
- First target compile failed on web SSID's undeclared bootId; changed to chip identity.
- Actual output exposed pinned platform's factory-offset/legacy boot_app0 bug. Project-local guard now removes the0xE000 legacy image and enforces app0x20000/2 MiB for generation/size/upload. Installed SDK was not patched. Separate release images preserve persistent partitions on normal updates. Combined factory images are not release update artifacts.
- A raw32-byte ascending-pattern scan matched SDK symbol `d_mult_table` at DROM3c10ebf0; it was a library permutation table, not a deployed pairing key. Replaced that misleading heuristic with fixture packet/text absence and explicit default-empty inventory/key tests. The public test key is not provisioned by firmware.

## Outstanding hardware acceptance

All are **AWAITING_HARDWARE**: actual board identification, supply/SPI/BUSY/DIO1/RF switch, one-node HELLO/challenge/telemetry/checkpoint, three-node coexistence, missing day over RF, gateway/node reboot continuity on real flash, brownout/interrupted NVS and filesystem writes, absent module/AP responsiveness, runtime memory at full inventory, load/collision/range/capacity measurements. Follow COMMISSIONING.md and record actual logs/captures. No zero-bug, range, zero-loss or RF pairing claim is made.
