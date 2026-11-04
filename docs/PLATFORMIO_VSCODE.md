# PlatformIO on Visual Studio Code — ESP32-C3 gateway

Open **this repository's root directory**, not the `firmware/` directory, in Visual Studio Code. The project root contains `platformio.ini`, while the application source lives in `firmware/` through the explicit `src_dir = firmware` setting. The recommended extension is **PlatformIO IDE**; VS Code can use the same PlatformIO Core as the terminal. This configuration targets the **ESP32-C3-DevKitM-1** with 4 MB flash and no PSRAM, not an ESP32-C6 and not the node's ESP32-S3.

![PlatformIO folder and build flow](assets/platformio-layout.svg)

After installing PlatformIO IDE, select the `esp32c3` environment in **Project Tasks → esp32c3 → General → Build**. To upload an identified gateway connected to the PC, use **Upload**, and use **Monitor** at 115200 baud. Do not assume an arbitrary COM port is the correct device. Power and verify the board/radio wiring and connect a matched antenna before enabling LoRa TX. If your C3 board has a different module, GPIO layout or flash capacity, do not select another board ID until its pin mapping, oscillator and partition table have been reviewed.

The same actions can be reproduced in the root terminal:

```powershell
python -m platformio run -e esp32c3
python -m platformio device list
python -m platformio run -e esp32c3 -t upload --upload-port COM7
python -m platformio device monitor --port COM7 --baud 115200
```

**COM7 is an example only.** Substitute the actual identified serial port from the device list. On Windows, install any USB-to-UART driver required by the exact development board. Radio operation does not require PSRAM, and an ESP32-S3 firmware image must never be flashed to this ESP32-C3 gateway.

The PlatformIO platform URL is pinned to **pioarduino 55.03.311**, which is the same platform release used by the read-only node repository at the audited commit. Both share **RadioLib 7.8.1** and **ArduinoJson 7.3.1**. Their board definitions are intentionally different: `esp32-s3-devkitc-1` for the meter node and `esp32-c3-devkitm-1` for the gateway. The upstream node contains additional OLED, RTC and graphics libraries that the gateway does not need. Do not copy those dependencies into the gateway.

The gateway is a single-core application. The radio task is the sole owner of SX1262 SPI, the storage task is the sole owner of persisted gateway state, and the HTTP task communicates through queues and snapshots. The pinned PlatformIO platform requires `tools/pio_flash_guard.py` because its default factory-image upload arguments may otherwise include `boot_app0.bin` at address `0xE000`, where the gateway maintains its own sequence ledger. **Do not remove this guard or change the partition file casually.** A routine app update should not include an erase or `uploadfs`; the latter overwrites stored checkpoint archives. See [flash layout and wiring](HARDWARE.md).

The optional interoperability check can compare both repositories without writing anything to the node:

```powershell
python tools/check_compatibility.py
python tools/check_compatibility.py --node-platformio "../esp32-modbus-energy/firmware/platformio.ini"
python tools/test_native.py
```

When the React dashboard is changed, run `npm ci`, `npm run build` and `npm test` in the `dashboard/` folder before compiling the firmware. The generated `firmware/web/assets.h` must match the React source. CI verifies the generated asset against the checked-in version so a clean firmware build does not silently include an outdated interface.

Common issues: **"No such package"** usually means the PlatformIO package download or cached platform installation failed; confirm network access and that the exact pinned platform release remains available. **"Unknown environment"** usually means the wrong folder is open in VS Code. **"No serial data"** means verifying the selected COM port, 115200 baud, bridge driver and board reset. **LoRa initialized but no telemetry** requires checking supply, DIO1/BUSY/NSS/SPI wiring, node/gateway PHY settings, matching pairing keys, successful HELLO/ACK and the node's Modbus acquisition before treating it as an RF range problem.
