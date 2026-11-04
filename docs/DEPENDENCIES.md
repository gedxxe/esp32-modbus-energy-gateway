# Reproducible dependency baseline

| Component | Pin | Role / license |
|---|---|---|
| PlatformIO Core | 6.1.19 (build host) | Build orchestration |
| pioarduino espressif32 | 55.03.311 release URL | C3 board/SDK integration |
| Arduino-ESP32 | 3.3.11 (platform pin) | Framework, LGPL-2.1-or-later |
| ESP-IDF | 5.5.5 (framework libs baseline) | ESP runtime, Apache-2.0 |
| RISC-V GCC | esp-14.2.0_20260121 | C3 compiler, GPL with runtime exception |
| RadioLib | 7.8.1 | Matches node dependency; MIT |
| ArduinoJson | 7.3.1 | Bounded API JSON; MIT |
| React / React DOM | 18.3.1 | Local dashboard; MIT |
| esbuild | 0.25.12 | Bundle/minify, build host only; MIT |
| jsdom | 26.1.0 | Production-bundle regression, host only; MIT |

`dashboard/package-lock.json` captures all transitive UI packages/integrity hashes. `platformio.ini` pins platform URL and radio/JSON versions. Do not replace RadioLib without rerunning vector, scheduler airtime and RF compatibility checks. No RTOS library is added: use ESP32's supplied FreeRTOS with bounded queues and unpinned tasks.

No node_modules, `.pio`, toolchain archives, reference cache, production credentials or development Python tools are included in the release source package. `firmware/web/assets.h` is a generated, local compressed asset; it embeds React's retained notices and no test fixture. A production firmware build does not serve `dashboard/preview.mjs` or test data.

The CI workflow provides current build and native-test evidence. Compiled release files are generated artifacts and are intentionally excluded from Git; do not treat an old release manifest as evidence for a newer source commit. Runtime heap/stack watermarks, flash power-fail behavior and RF performance need the hardware acceptance procedure.

## Release hygiene (gateway ESP32-C3)

The gateway release generator now packages tracked source and verified flash images, including VS Code project settings and the PlatformIO root configuration. It excludes `.elf` debugger output, which can contain build-machine paths, and reports actual SHA256 hashes instead of old hardcoded software test counts. The pinned upstream node package contract is documented in `docs/upstream-package-lock.json`. CI rechecks this record, source and wiring diagrams, native tests, React bundle parity, ESP32-C3 firmware build, filesystem size and flash partitions. Packaging does not flash or modify any hardware.
