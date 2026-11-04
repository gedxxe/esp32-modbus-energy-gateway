<p align="left"><img src="assets/midea-brand.svg" alt="Midea" width="185" /></p>

![PlatformIO](assets/badges/platformio.svg) ![C++17](assets/badges/cplusplus.svg) ![RadioLib](assets/badges/radiolib.svg)

# Gateway verification and reproducible evidence

[← README](../README.md) · [PlatformIO](PLATFORMIO_VSCODE.md) · [Collision control](COLLISION_CONTROL.md) · [Flash safety](COMMISSIONING.md)

**The new gateway project root is `firmware/`**, containing `platformio.ini`, `src/`, `include/` and `partitions.csv`. The companion ESP32-S3 node follows the same PlatformIO convention but has a different board. GitHub Actions validates software, not physical RF performance.

<details open>
<summary><b>Local reproducibility commands</b></summary>

```powershell
# From gateway Git repository root
python tools/security_audit.py --history
python tools/check_compatibility.py
python tools/check_wiring.py
python tools/test_native.py

cd dashboard
npm ci
npm run build
npm test
cd ..

python -m platformio run --project-dir firmware -e esp32c3
python -m platformio run --project-dir firmware -e esp32c3 -t buildfs
python tools/check_flash_layout.py
python tools/package.py
python tools/package.py --verify
```

For VS Code, open **`firmware/`** and click **PROJECT TASKS → esp32c3 → Build** instead. The `firmware/include/web/assets.h` bundle must match the React generator. `tools/pio_flash_guard.py` protects the 0xE000 sequence journal from the ESP32-C3 factory boot image. A routine upgrade must not overwrite the LittleFS archives.

</details>

<details>
<summary><b>Regression coverage and package checks</b></summary>

The native tests compile the **production** gateway codec/HMAC, per-node replay and session validation, archived day reconstruction and CRC, scheduler fairness/ACK reserve, CAD deferral policy, downlink sequence reservations across power cycles, queue capacity and four-node identity separation. The independent public wire vector is cross-checked against Python SHA256. The dashboard tests verify authentication, missing/stale kWh semantics and embedded gzip parity. CI also parses generated SVG images, wiring netlists and PlatformIO package versions.

Production firmware is compiled for **`esp32-c3-devkitm-1`** using pinned pioarduino **55.03.311**, RadioLib **7.8.1**, ArduinoJson **7.3.1**. After the build, the actual partition/image map and source+flash ZIP SHA256 are checked. The ZIP intentionally excludes ELF debugging data. See [CI workflow](../.github/workflows/ci.yml) and [GitHub Actions](https://github.com/gedxxe/esp32-modbus-energy-gateway/actions) for the **exact commit's** results.

</details>

<details>
<summary><b>Unverified physical conditions</b></summary>

**Still awaiting actual radios:** ESP32-C3/ESP32-S3 electrical supply and antenna checks, RF switch and oscillator confirmation, four-node PTMP throughput under real interference, CAD outcomes, gateway ACK latency, archive durability after sudden power loss, Modbus readings at real meters, and installation-range evidence with walls. Software-only tests must not be cited as physical collision-free or industrial-grade acceptance.

</details>
