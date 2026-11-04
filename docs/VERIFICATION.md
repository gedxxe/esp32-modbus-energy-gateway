# Gateway verification

The reference gateway targets ESP32-C3-DevKitM-1 with an SX1262 Ra-01SH V1.1. The current receiver is designed to interoperate with Energy Station LoRa protocol v1 from `gedxxe/esp32-modbus-energy`. Results from a software regression suite establish behavior of the host test harness; they must not be interpreted as measurements of RF range or verified electrical energy readings.

## Reproducible checks

From the repository root, use `python tools/security_audit.py` to examine tracked files for private workstation paths and common credential formats. Then execute `python tools/test_native.py` to exercise the actual gateway codec, HMAC, replay handling, sequence reservation, checkpoint decoder, radio scheduler and automated refresh policy. The tests include multiple node identities, invalid packet authentication, repeated frames and lost response cases. Any status claim must refer to a specific commit and a completed test run. `node --test` in `dashboard/` runs the UI tests after `npm ci` and `npm run build`.

The dashboard build regenerates `firmware/web/assets.h`, which is compiled into the ESP32 web server. The firmware target is built using `python -m platformio run -e esp32c3`. The corresponding CI workflow builds the browser bundle, verifies its tests, checks the native code and then builds the gateway firmware. Always check that the generated web bundle is current. Release binaries and ELF files should be generated as CI artifacts rather than copied into the source repository.

## Physical acceptance still required

The software cannot verify a particular ESP32-C3 pinout, SX1262 crystal vs TCXO requirements, RF switch, installation antenna or factory coverage. Commissioning must first record bidirectional HELLO/ACK exchange with one node, successful candidate-session confirmation, manual POLL mode 0 followed by correlated TELEMETRY, matching kWh values from the node and gateway, retries, timeouts and RF quality. Repeat with four nodes at the selected interval, disable one node during a request, reboot the gateway, and confirm that persisted checkpoint archives remain intact. The gateway's 1% airtime policy and randomized node backoff reduce channel load but cannot guarantee collision-free operation. Physical acceptance remains open until those results exist.
