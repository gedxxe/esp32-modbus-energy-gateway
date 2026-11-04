# Midea Engineering — LoRa Power Monitoring Gateway

[Dokumentasi Bahasa Indonesia](README.id.md) · [Protocol and scheduling](docs/RADIO_DESIGN.md) · [Commissioning](docs/COMMISSIONING.md) · [Node compatibility](docs/NODE_COMPATIBILITY.md)

This project is the receiver for the [ESP32 power-meter node](https://github.com/gedxxe/esp32-modbus-energy). The node reads a meter over Modbus RTU, validates the reading and applies the meter's PT/CT ratios. This ESP32-C3 gateway listens for authenticated LoRa frames and exposes the received values on a local dashboard. The two repositories share the **Energy Station protocol v1**, not a shared firmware image. The node repository is the authoritative source for the radio packet format. Changes in this repository do not remotely update any node.

## Reading power-meter data

The dashboard keeps the last valid kWh reading visible with its source timestamp, receive age, meter profile, inventory ID and radio link status. A reading becomes **stale** as time passes, but the value is not erased or replaced by zero. A valid LoRa payload already contains scaled engineering kWh; the gateway **must never multiply by PT/CT again**. The current protocol does not transmit a full register map or every electrical measurement. Other parameters should be added only through a versioned change agreed by both node and gateway; inventing fields in the gateway would produce misleading data.

The operator can click **Request live** for a particular node. This asks the node to send the most recent Modbus-derived sample it currently holds; it does not command a new RS485 register transaction. The gateway sends an authenticated LoRa `POLL` with mode 0, and the node responds with a `TELEMETRY` frame containing the matching request sequence. This is an application-level request/response exchange, similar in intent to a read-only HTTP GET, but it is not HTTP transported over LoRa. The request is queued, then transmitted subject to the radio duty cycle and other pending packets. The dashboard distinguishes a queued request, an outstanding response, a received measurement and a timeout; merely accepting the HTTP command is not proof that the meter was read.

## One gateway, several nodes

A node can be configured locally to transmit once every hour. Because LoRa is half-duplex, simultaneous uplinks can interfere; assigning a one-hour interval to all nodes does **not** give each of them a guaranteed time slot. The gateway gives ACK frames priority, limits its own airtime, uses round-robin ordering for queued transmissions and does not start several stale-data POLLs together. If no valid update arrives for an hour plus a grace period, it can issue a low-priority refresh request. Unsuccessful automatic requests are rate-limited. These controls reduce congestion but are not TDMA and cannot eliminate collisions caused by independent transmitters. Full airtime and recovery assumptions are recorded in [radio design](docs/RADIO_DESIGN.md).

Each node must have a distinct radio ID and its own 32-byte HMAC key. The gateway supports up to 16 inventory entries, which is a storage limit rather than a radio capacity guarantee. Keys are configured locally, never sent in LoRa packets and are not returned through the dashboard API. LoRa payloads are authenticated but **not encrypted**; they should not contain personal data. Local checkpoint archives remain separate from the latest telemetry and are verified before being stored.

## Hardware and setup

The reference hardware is an **ESP32-C3-DevKitM-1 with an SX1262 Ra-01SH V1.1**. This is a board-specific reference, not a claim that an unknown module will work without checking wiring, RF-switch configuration and oscillator supply. The gateway starts with radio disabled and no enrolled nodes. The local commissioning AP and access code are announced on the Serial Monitor at 115200 baud, while the dashboard is served at `http://192.168.4.1`. The node's published recovery AP password remains a separate node-firmware setting; the gateway's commissioning credentials are not interchangeable with that password.

From the repository root, with Python 3, PlatformIO Core and a C++ compiler available:

```sh
python -m platformio run -e esp32c3
python tools/test_native.py
```

To build the locally bundled dashboard, use Node.js 22 and run `npm ci`, `npm run build` and `npm test` inside `dashboard/`. The build regenerates `firmware/web/assets.h`; build the firmware **after** generating the dashboard. Normal firmware updates should not overwrite a device's archive partition. See [commissioning](docs/COMMISSIONING.md) before flashing or clearing flash.

## Security and acceptance

This is a local engineering installation, not an Internet-exposed service. The dashboard uses HTTP on a controlled AP and a temporary commissioning access code. At-rest flash encryption, a trusted gateway RTC, cloud synchronization and a managed multi-user login system are not provided. Host software tests and ESP32 compilation do not prove radio range, collision-free operation or accurate readings on a particular meter. The final acceptance procedure must include a real node, gateway and antenna installed at the intended locations. Repository hygiene checks are in `tools/security_audit.py`; local workstation paths, provisioning keys, compiled ELF files and temporary build output do not belong in Git.

[Source layout](docs/ARCHITECTURE.md) · [Validation](docs/VERIFICATION.md) · [Hardware](docs/HARDWARE.md) · [Licenses](NOTICE)
