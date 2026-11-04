<p align="center"><img src="docs/assets/midea-brand.svg" alt="Midea" width="240" /></p>

<h1 align="center">Energy Station · LoRa PTMP Gateway</h1>
<p align="center">ESP32-C3 + SX1262 · authenticated Modbus-energy telemetry · offline web dashboard</p>

<p align="center">
<img src="docs/assets/badges/board.svg" alt="ESP32-C3 board" height="25" />
<img src="docs/assets/badges/platformio.svg" alt="PlatformIO 6.1.19" height="25" />
<img src="docs/assets/badges/protocol.svg" alt="Energy Station LoRa v1" height="25" />
<img src="docs/assets/badges/firmware.svg" alt="Gateway 0.3.0-rc1" height="25" />
</p>
<p align="center">
<img alt="C++17" src="https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white&style=flat-square" />
<img alt="Arduino" src="https://img.shields.io/badge/Arduino-ESP32%203.3.11-00878F?logo=arduino&logoColor=white&style=flat-square" />
<img alt="JavaScript" src="https://img.shields.io/badge/JavaScript-ES2020-F7DF1E?logo=javascript&logoColor=black&style=flat-square" />
<img alt="React" src="https://img.shields.io/badge/React-18-61DAFB?logo=react&logoColor=black&style=flat-square" />
<img alt="License" src="https://img.shields.io/badge/License-GPL--3.0-555?style=flat-square" />
<img alt="GitHub stars" src="https://img.shields.io/github/stars/gedxxe/esp32-modbus-energy-gateway?style=flat-square" />
</p>

<p align="center"><a href="README.id.md">Bahasa Indonesia</a> · <a href="docs/PLATFORMIO_VSCODE.md">PlatformIO / VS Code</a> · <a href="docs/COMMISSIONING.md">Commissioning</a> · <a href="docs/COLLISION_CONTROL.md">Collision control</a> · <a href="https://github.com/gedxxe/esp32-modbus-energy">ESP32-S3 node</a></p>

## What this firmware does

The ESP32-C3 receives **authenticated Energy Station protocol v1** packets from multiple ESP32-S3 power-meter nodes over SX1262 LoRa. Each node acquires Modbus RTU data over its own RS485 bus, validates the reading and applies CT/PT scaling **at the node**. The gateway does **not** scale energy again: it correlates packets, checks HMAC and replay windows, preserves the last valid kWh and archives validated capture records. A browser connects to the local Wi-Fi access point for monitoring and commissioning. Each inventory entry has its own network/node ID and 32-byte key.

The radio uses **one shared half-duplex channel**. The gateway prioritizes acknowledgments and performs nonblocking SX1262 CAD before discretionary POLLs. A companion node firmware update adds CAD before node uplinks. Both sides use bounded random backoff, but **hidden-node collisions remain possible**. No false claims of collision-free TDMA, guaranteed 16-node throughput, or field acceptance are made.

## Explore the system

<table><tr><td width="50%" align="center"><a href="docs/assets/ptmp-topology.svg"><img src="docs/assets/ptmp-topology.svg" alt="LoRa PTMP node-to-gateway topology" width="450" /></a><br /><b>01 · PTMP topology</b><br /><a href="docs/NODE_COMPATIBILITY.md">Protocol and peer firmware</a></td><td width="50%" align="center"><a href="docs/assets/wiring-c3.svg"><img src="docs/assets/wiring-c3.svg" alt="ESP32-C3 to SX1262 wiring" width="450" /></a><br /><b>02 · C3 wiring</b><br /><a href="docs/HARDWARE.md">GPIO, module pads and power</a></td></tr><tr><td align="center"><a href="docs/assets/poll-sequence.svg"><img src="docs/assets/poll-sequence.svg" alt="Telemetry and POLL sequence" width="450" /></a><br /><b>03 · Request / response</b><br /><a href="docs/RADIO_DESIGN.md">Scheduling and correlation</a></td><td align="center"><a href="docs/assets/platformio-layout.svg"><img src="docs/assets/platformio-layout.svg" alt="VS Code PlatformIO project tree" width="450" /></a><br /><b>04 · PlatformIO workspace</b><br /><a href="docs/PLATFORMIO_VSCODE.md">Open firmware/ and build</a></td></tr><tr><td align="center"><a href="docs/assets/channel-access.svg"><img src="docs/assets/channel-access.svg" alt="SX1262 channel access" width="450" /></a><br /><b>05 · Collision-aware access</b><br /><a href="docs/COLLISION_CONTROL.md">CAD, retries and limitations</a></td><td align="center"><a href="docs/assets/gateway-architecture.svg"><img src="docs/assets/gateway-architecture.svg" alt="Gateway firmware architecture" width="450" /></a><br /><b>06 · Core architecture</b><br /><a href="docs/ARCHITECTURE.md">Radio/storage/web ownership</a></td></tr></table>

*Each preview opens the full SVG. The SVG files contain linked sections for viewers supporting embedded links; GitHub's inline image renderer may disable those links. The expandable sections below work directly in GitHub Markdown.*

## VS Code / PlatformIO — open the correct folder

**Select `esp32-modbus-energy-gateway/firmware/` using File → Open Folder.** The `platformio.ini` is **inside this folder**, exactly like the ESP32-S3 node. Do not open the repository root as the PlatformIO project.

```text
esp32-modbus-energy-gateway/
├── firmware/               ← OPEN THIS FOLDER IN VS CODE
│   ├── platformio.ini       ← [env:esp32c3]
│   ├── partitions.csv       ← 4 MB ESP32-C3 flash
│   ├── src/                 ← PlatformIO automatically compiles *.cpp recursively
│   │   ├── main.cpp
│   │   ├── app/  core/  radio/  storage/  web/
│   ├── include/             ← headers and bundled offline dashboard
│   │   ├── app/  board/  core/  storage/  web/
│   └── .vscode/             ← recommended extension + IntelliSense
├── dashboard/               ← React source, tests, asset generator
├── docs/                    ← SVG diagrams + hardware/protocol guides
├── tests/                   ← production-core host tests
└── tools/                   ← host-only QA, package and flash guard
```

<details open>
<summary><b>Build / upload from VS Code</b></summary>

Install **PlatformIO IDE** in VS Code, open the `firmware/` folder and select **PROJECT TASKS → esp32c3 → Build**. The project uses the Arduino framework, board `esp32-c3-devkitm-1`, pinned pioarduino `55.03.311`, RadioLib `7.8.1` and ArduinoJson `7.3.1`. Upload only after identifying the correct serial port and checking wiring, voltage and antenna. The source node project uses the different `esp32-s3-devkitc-1` target.

```powershell
# From inside the firmware/ folder
python -m platformio run -e esp32c3
python -m platformio device list
python -m platformio device monitor --baud 115200

# Equivalent from the repository root
python -m platformio run --project-dir firmware -e esp32c3
python tools/test_native.py
python tools/check_compatibility.py
python tools/check_wiring.py
```

</details>

<details>
<summary><b>Radio identity, PHY and PTMP assumptions</b></summary>

| Setting | Gateway reference default | Notes |
| --- | --- | --- |
| MCU | ESP32-C3-DevKitM-1 | 4 MB, no PSRAM |
| Module | Ai-Thinker Ra-01SH V1.1, SX1262 | Crystal; DIO2 RF switch |
| Carrier | **915 MHz** | Configurable on both endpoints; verify authorization for the site |
| Bandwidth / SF / coding | 125 kHz / SF7 / 4/5 | Must match all nodes |
| Preamble / logical sync | 8 / 0x12 | Explicit header, CRC and normal IQ |
| Configured TX power | **14 dBm** | Software range 0–22 dBm; conducted power is not EIRP |
| IDs | Network 1 / gateway 1 | Node IDs must be different and unique |
| Inventory | Up to 16 nodes | Hardware/load capacity must be measured |
| Wire protocol | Energy Station v1 | 24-byte header + payload ≤112 + 16-byte truncated HMAC |

The 915 MHz carrier and 14 dBm are the actual source defaults. A Moxon antenna changes EIRP through its gain and cable losses; calculate measured compliance separately in [RF engineering reference](docs/RF_COMPLIANCE_ID.md).

</details>

<details>
<summary><b>Data semantics and safety</b></summary>

**Request live** asks for the node's most recently available measurement; protocol v1 does not command a new Modbus FC03 transaction. A stale value keeps the last valid kWh with its age. Missing or invalid energy is **not zero**. A radio ACK means the receiver accepted the packet; it does **not** prove checkpoint persistence. Local HTTP and boot commissioning access are for a controlled network, not Internet exposure.

**Flash safety:** `gw_nvs` at 0xE000 contains a persistent POLL sequence ledger. The factory application starts at 0x20000. The archives partition starts at 0x220000. **Do not use `uploadfs` for routine firmware upgrades**: it can overwrite saved checkpoints. The checked release ZIP excludes ELF debugger output to avoid embedding machine paths.

</details>

## Validation and source references

The gateway [CI workflow](.github/workflows/ci.yml) verifies the C3 PlatformIO build, source-level node protocol compatibility, firmware/native tests, the bundled React app, generated SVG integrity, and flash-image layout. The node-side CAD development is tracked separately in [node PR #5](https://github.com/gedxxe/esp32-modbus-energy/pull/5). Both PRs require physical multi-radio commissioning before being treated as an industrial deployment.

[Architecture](docs/ARCHITECTURE.md) · [Power / wiring](docs/HARDWARE.md) · [Protocol](docs/NODE_COMPATIBILITY.md) · [Radio strategy](docs/RADIO_DESIGN.md) · [RF reference](docs/RF_COMPLIANCE_ID.md) · [CI evidence](docs/VERIFICATION.md) · [License](LICENSE)

---
<sub>Midea Engineering — independent source review and validation. Hardware measurements, interference performance and RF authorization are deployment-specific.</sub>
