# Energy Station LoRa Gateway

[Bahasa Indonesia](README.id.md) · [Commissioning](docs/COMMISSIONING.md) · [Compatibility](docs/NODE_COMPATIBILITY.md) · [Work tracker](WORK_STATUS.md)

Independent raw-LoRa PTMP gateway for `gedxxe/esp32-modbus-energy` firmware3.4.3, Energy Station protocol1. First build target: **ESP32-C3-DevKitM-1 + SX1262 Ai-Thinker Ra-01SH V1.1**, 4 MB flash, no PSRAM. Board configuration is isolated. The node repository is a read-only compatibility reference pinned to commit26c3a9429c8c4d410ef11a1b81e1e42f295034b1.

The MVP implements authenticated packets, ACK priority/fair scheduling with a shared1% TX gate, candidate-session live challenges, durable POLL sequence reservations, telemetry, schema3 checkpoint reassembly/validation and immutable local archives. A locally bundled **React** AP console provides inventory, PHY settings, freshness, live/checkpoint requests, archive JSON export, bounded logs and layered diagnostics. English is default with an Indonesian option. No CDN or internet connection is needed.

Radio starts disabled; inventory starts empty and has16 slots. Gateway/default network IDs are1; examples2/3/4 require distinct commissioned keys. Slots are not an RF capacity promise. Missing/stale values never become measured zero. HMAC authenticates, not encrypts. No cloud, server database, enterprise Wi-Fi, mesh, LoRaWAN, OTA or remote node-configuration extension is included.

## Build

The0.1.1 dashboard uses a responsive grid of cards per `(Network ID, Node ID)` on Overview and Node inventory. Each card shows session/data state, kWh, receive/sample ages, observed RSSI/SNR, latest accepted frame/sequence, and separate new/duplicate RX counters. Expand **Received data** for the decoded source fields. A **+ Add node** card suggests an unused address, rejects conflicting IDs and opens the existing authorized flash-inventory save flow. Saved IDs/names/keys survive reload/reboot through verified NVS banks; latest live data and link counters remain RAM-only. Browser ages continue advancing when status polling stalls. See [card preview](docs/assets/node-cards-mobile-qa.png) (simulated data).

Prerequisites: Python3 + PlatformIO Core6.1.19; pins in platformio.ini. Host terminal INPUT, repository root:

```powershell
python -m platformio run -e esp32c3
python -m platformio run -e esp32c3 -t buildfs
python tools/test_native.py
```

On this Windows workstation, PlatformIO is installed at `C:/Users/Lenovo/.platformio/penv/Scripts/platformio.exe`; use that executable if the current Python cannot import PlatformIO. Native tests require g++. Source paths containing spaces are passed as argument arrays. Generated dashboard assets are checked in. To edit/rebuild the UI, host terminal INPUT from `dashboard/`:

```powershell
npm ci
npm run build
npm test
```

Use Node22/npm with package-lock.json. Compile firmware after rebuilding UI. `npm run preview` serves a labelled simulated fixture using the actual production bundle at127.0.0.1:4173; it is UI QA, not RF evidence.

Release binaries, verified hashes, compile sizes and exact test outcomes are documented in [VERIFICATION](docs/VERIFICATION.md). `python tools/package.py` creates the reproducible manifest/source package after successful builds; `--verify` checks it. Never upload filesystem images during a routine update: they replace historical archives. No device is flashed automatically.

## Use

Follow [hardware/wiring](docs/HARDWARE.md), verify purchased board/module and antenna, then first-provision deliberately. Serial Monitor **OUTPUT** at115200 prints the AP name, new boot-only WPA2 password and commissioning access code. Open192.168.4.1 on that AP. Register a node/key, save, match shared PHY, then enable radio. Save/queue/apply results are separate from driver initialization, HELLO ACK, confirmed session and fresh data.

Use periodic uplinks first. Request a live value only when needed; checkpoint mode0-day selects the node's active capture day. JSON exports retain ten WIB points with source UTC, historical profile/meter/ratios and gateway monotonic receive reference. Gateway has no trusted UTC source; no DS3231 is assumed. Latest live telemetry is RAM-only; durable checkpoints use64 bounded append-only slots.

Read [commissioning/recovery](docs/COMMISSIONING.md) before reboot, storage repair or inventory identity changes. A lost/corrupt sequence ledger blocks POLL; never reset sequence to1 for a node session that still remembers prior polls. Physical RF, power-fail, range and multi-node capacity acceptance remain **AWAITING_HARDWARE**, independently of compile/native tests.

## Source map

| Directory/file | Responsibility |
|---|---|
| firmware/board | GPIO, oscillator and RF-switch adapter |
| firmware/core | Portable production codec/HMAC, schema, sessions, replay, scheduler |
| firmware/storage | CRC banks, sequence reservation, bounded immutable archives |
| firmware/app, radio | FreeRTOS owner tasks/queues and sole SPI/RadioLib owner |
| firmware/web, dashboard | Bounded authorized API and locally built React UI |
| tests, tools | Production-code regressions, vector check, packaging |
| AGENTS.md, WORK_STATUS.md | Permanent rules and resumable work/evidence |
| docs | Decisions, provenance, ownership diagram, wiring/netlist, acceptance |

See [architecture/ownership](docs/ARCHITECTURE.md) and [decisions](docs/DECISIONS.md). GPL-3.0-only; upstream adaptations and bundled dependency notices are preserved in LICENSE, NOTICE and licenses/.
