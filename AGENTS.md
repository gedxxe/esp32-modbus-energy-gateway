# Gateway engineering rules

- Scope: raw LoRa PTMP Energy Station v1 and local commissioning AP only. No cloud, enterprise Wi-Fi, LoRaWAN, mesh, OTA, or backend expansion.
- First target is ESP32-C3 (user correction, 2026-11-04). It has one application core; use unpinned tasks. Board pins are isolated from protocol/application code. Physical board/module identification and RF tests are outstanding until recorded with evidence.
- Read this file, WORK_STATUS.md, git status, and docs/NODE_COMPATIBILITY.md before continuing work. Track actual owners only.
- Protocol authority: node commit 26c3a9429c8c4d410ef11a1b81e1e42f295034b1, implementation plus public vector. Firmware 3.4.3 is not protocol version 1. Never silently change the wire format or node repository.
- Explicit little-endian codecs only; bounded lengths and key lookup; HMAC-SHA256 truncated to 16 bytes with constant-time comparison. No unknown-node enrollment. Keys are never returned through APIs/logs/browser persistence; public vector keys are test-only.
- One task owns SPI/radio. ISR only signals. HTTP queues commands and reads snapshots, never touches radio. One storage owner performs verified writes and reservations before TX. Keep queues, logs, replay state, inventory (16), and archives bounded.
- ACK echoes the node session/sequence and type with addresses swapped. Valid duplicates are ACKed without duplicate insertion. HELLO is a candidate until a correlated fresh live response proves activity; preserve established data while challenging candidates.
- POLL sequences must be reserved durably before use and never reused within a node session after reboot. Lost/corrupt state fails closed; no invented remote reset commands.
- Shared gateway airtime gate covers ACK and POLL; prioritize ACK and fairness; one global checkpoint transfer. Packet accepted, record validated, and record durable are distinct statuses.
- Storage never auto-formats. Versioned CRC records, verified staging/banks, bounded capacity; preserve historical source identity after inventory edits. Missing/stale/invalid values are not measured zero. UTC is unavailable until actually trusted.
- Do not claim encryption at rest, RF success, zero loss, range, or hardware pairing without evidence. Separate software completion and AWAITING_HARDWARE in the tracker.
- Keep source provenance/license for reused node code; no dependency directories/caches or installation credentials in deliverables.

## Verified commands

- `C:/Users/Lenovo/.platformio/penv/Scripts/platformio.exe run -e esp32c3` — Arduino-ESP32 3.3.11 / pioarduino55.03.311 / RadioLib7.8.1; use `python -m platformio` on hosts with Core6.1.19 installed in that interpreter.
- `C:/Users/Lenovo/.platformio/penv/Scripts/platformio.exe run -e esp32c3 -t buildfs` — builds first-provisioning LittleFS image; never upload it over historical records casually.
- `python tools/test_native.py` — actual production C++ with g++ and independent Python HMAC/vector checks. This workstation's MinGW needs normal host execution because the sandbox causes DLL relocation failure.
- From `dashboard/`: `npm ci`, `npm run build`, `npm test` — Node22, lockfile pinned; test actual generated assets. Rebuild UI before firmware when UI changes.
- `python tools/check_flash_layout.py` — run after firmware/buildfs; factory offset/capacity and preservation guard are mandatory. Use supported esptool upload only. Never use combined factory images for updates; release supplies separate images.
- `python tools/package.py` and `python tools/package.py --verify` — run after successful checks; source manifest must be regenerated after final source/docs/tracker edits.
