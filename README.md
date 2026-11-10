# ESP32 Modbus Energy — LoRa Gateway

Firmware for an **ESP32-C3 + SX1262** gateway receiving power-meter readings from the separate [ESP32-S3 node firmware](https://github.com/gedxxe/esp32-modbus-energy).

This repository currently provides a **local Wi-Fi access point and dashboard**. It does **not** implement a Wi-Fi station connection, PostgreSQL upload, or an uplink to the Midea website. The Heltec ESP32-S3 SX1262 V3 gateway is not the current build target.

## Data flow

```text
Power meter → RS485/Modbus → ESP32-S3 node → SX1262 LoRa
           → ESP32-C3 gateway → Local Wi-Fi AP → Browser
```

The node reads and scales the meter's kWh value. The gateway authenticates LoRa packets, tracks node sessions, acknowledges accepted packets, serves the latest data, and stores requested checkpoint records in LittleFS. It does **not** repeat CT/PT scaling.

[Topology diagram](docs/assets/ptmp-topology.svg) · [Radio protocol](docs/NODE_COMPATIBILITY.md) · [Firmware design](docs/ARCHITECTURE.md)

## Build

Open **`firmware/`** in VS Code with PlatformIO. Do not open the repository root as the PlatformIO project.

| Setting | Current firmware |
| --- | --- |
| Board | `esp32-c3-devkitm-1` (ESP32-C3, 4 MB flash) |
| Radio | SX1262 / Ra-01SH V1.1 |
| Platform | pioarduino `55.03.311` |
| RadioLib / ArduinoJson | `7.8.1` / `7.3.1` |
| LoRa protocol | Energy Station v1 |
| Default PHY | 915 MHz, BW125, SF7, CR4/5, preamble 8, sync 0x12, TX 14 dBm |
| Node inventory | Up to 16 configured entries; not a tested RF capacity |

From the repository root:

```powershell
python -m platformio run --project-dir firmware -e esp32c3
python tools/test_native.py
python tools/check_compatibility.py
```

The dashboard source is under `dashboard/`. If it changes, regenerate its embedded firmware asset **before** building:

```powershell
cd dashboard
npm ci
npm run build
npm test
```

See [PlatformIO instructions](docs/PLATFORMIO_VSCODE.md) and [gateway setup](docs/COMMISSIONING.md) for uploading and first-time configuration.

## How to use it

1. Connect to the gateway's local Wi-Fi AP. Its password and gateway access code appear in the Serial Monitor at 115200 baud after boot.
2. Add each node with its own network ID, node ID, and matching 32-byte key. Select the same radio settings as the node and enable the gateway.
3. Confirm an authenticated node session before requesting data. **Request live** asks for the node's most recently available measurement; it does not start a fresh Modbus read.
4. Use **Request checkpoint** for a stored capture day. ACK confirms packet acceptance, **not** a successful flash write.

The latest valid kWh can remain visible when a newer packet is invalid or missing; its age must be checked. Missing energy is not zero.

## Limits that matter

- One shared, half-duplex LoRa channel. CAD and random backoff reduce some collisions but do not eliminate hidden-node collisions or guarantee throughput.
- The gateway applies a per-boot 1% TX airtime pacing policy. Higher node counts or frequent reports can delay ACKs. Default client firmware settings must be evaluated against the actual node count.
- Local archive capacity is **64 checkpoint records per gateway**. Old records are not removed automatically.
- The persistent POLL sequence ledger retains **64 node/session identities**. If it is full or unsafe, POLL fails closed; do not erase it to bypass replay protection.
- A routine firmware upgrade must preserve `gw_nvs` and the archive partition. **Do not run `uploadfs` on a device with records you need to retain.**
- Protocol v1 authenticates packets with HMAC but does not encrypt readings. The local HTTP dashboard is not intended for public Internet access.
- The 915 MHz default is a software setting, **not** permission to transmit at the installation site.

The current CI checks the gateway build and its pinned client revision. This does not establish compatibility with every newer client commit or physical radio performance. See [current development status](WORK_STATUS.md), [test details](docs/VERIFICATION.md), [radio behavior](docs/COLLISION_CONTROL.md), and [RF reference](docs/RF_COMPLIANCE_ID.md).

[README Bahasa Indonesia](README.id.md) · [License](LICENSE)
