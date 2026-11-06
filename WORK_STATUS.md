<p align="left"><img src="docs/assets/midea-brand.svg" alt="Midea" width="185" /></p>

![Gateway](docs/assets/badges/firmware.svg) ![C3](docs/assets/badges/board.svg) ![Protocol v1](docs/assets/badges/protocol.svg)

# Gateway development status

## Baseline and scope

Gateway work is based on `gedxxe/esp32-modbus-energy-gateway`, with the upstream `gedxxe/esp32-modbus-energy` firmware used as a read-only **main-branch** protocol baseline; the companion [node PR #5](https://github.com/gedxxe/esp32-modbus-energy/pull/5) proposes a separate CAD update. The upstream node at main commit `737cfff590c0cbea33895df17c3e3ffc06449e15` uses Energy Station radio protocol v1 and firmware version 3.5.0. The gateway remains a separate ESP32-C3 / SX1262 application. No changes to the node's Modbus register reading, operating settings or public recovery AP password are part of this gateway work.

## Implemented in source

The gateway has a fixed-length authenticated packet decoder, per-node HMAC keys, replay suppression, verified downlink sequence reservation, a radio task owning SPI/RadioLib, a local commissioning API and dashboard, checkpoint reassembly and archive storage. Manual refresh sends LoRa POLL mode 0 and receives a correlated TELEMETRY response. Current work also distinguishes the last valid kWh reading from the newest telemetry state, and introduces a conservative round-robin refresh policy for nodes that have missed their expected periodic report. The node still controls its own scheduled transmit interval.

## Pending evidence and follow-up

Run the updated UI build/tests, native tests, source audit and ESP32-C3 compilation against the new gateway branch. Confirm the actual generated dashboard bytes are embedded in firmware before flashing. Software tests cannot prove the selected antenna, radio range in the factory or reliability with all four installed nodes; bench commissioning with the source node is still required. Other measurements besides scaled kWh are not available in the current live LoRa v1 payload and must not be claimed on the dashboard.

Keep personal paths, authentication material, compiled ELF files, local cache directories and real network configuration out of source control. Historic commits containing earlier local paths remain accessible until history is deliberately rewritten and all clones, forks and references are handled. The path exposure is a privacy issue even where no password was involved. Architecture detail and physical acceptance procedures are recorded in `docs/RADIO_DESIGN.md` and `docs/COMMISSIONING.md`.

## PTMP PlatformIO re-audit (2026-11-05)

New gateway-only draft PR #2 pins the latest inspected read-only node commit `737cfff590c0cbea33895df17c3e3ffc06449e15`. The ESP32-C3 gateway and ESP32-S3 node share pioarduino 55.03.311, RadioLib 7.8.1, ArduinoJson 7.3.1 and Energy Station v1. The C3 board is `esp32-c3-devkitm-1` in the gateway root `platformio.ini`, and the purchased board/module must still be physically verified. Nonblocking SX1262 CAD and bounded randomized POLL backoff reduce gateway-originated interference but cannot prevent hidden-node simultaneous uplinks or provide TDMA. The generated SVG wiring/topology/request/build diagrams are checked by CI. A safe release ZIP deliberately excludes ELF symbols and old machine paths. All hardware RF/PTMP acceptance remains open and no node repository changes were made.

## PlatformIO workspace redesign

Gateway source has been reorganized into `firmware/src/` and `firmware/include/`, with `firmware/platformio.ini`, `firmware/partitions.csv` and `firmware/.vscode/`. Open `firmware/` directly in VS Code. The radio default remains 915 MHz and 14 dBm configured TX. Cross-compile and CI tests remain separate from hardware RF acceptance.

[View PlatformIO guide](docs/PLATFORMIO_VSCODE.md) · [CAD flow diagram](docs/assets/channel-access.svg) · [Wiring](docs/assets/wiring-c3.svg)

## Gateway-only peer sync re-audit (2026-11-06)

The current read-only node `main` at `3896a8b46bd2cd63f9756a46f0b4198a20aac6c2` has been compared against the gateway's LoRa v1 packet handling. The gateway audit branch extends CI to exercise the exact current peer as well as the original v1 baseline, adds CHECKPOINT frame/ACK interoperability coverage, and corrects slow-PHY duty-gate expiry handling for queued HELLO-session challenge POLLs and ACKs. These are gateway-only changes; node firmware was not modified. See [the dated peer audit](docs/LORA_PEER_AUDIT_20261106.md) and [CI checks](https://github.com/gedxxe/esp32-modbus-energy-gateway/actions). Physical gateway/node RF acceptance is still pending.
