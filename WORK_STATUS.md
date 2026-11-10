# Gateway development status

Updated: 2026-11-10. This file describes the source, not the status of installed hardware.

## Current gateway baseline

- Gateway `main`: `8f95c0e542bb4a198256cf235ce86c9bb8824efd` (merged PR #6, 2026-11-07).
- PlatformIO target: `esp32-c3-devkitm-1` + SX1262 Ra-01SH; firmware version label `0.3.0-rc1`.
- LoRa: Energy Station v1, per-node HMAC keys, session/replay checks, ACK scheduling, CAD before POLL, and bounded queues.
- Storage: local config/sequence banks, up to 64 immutable checkpoint records, and 64 retained node/session sequence entries.
- Dashboard: local ESP32 Wi-Fi AP + HTTP server. No gateway Wi-Fi STA, PostgreSQL uplink, or Midea website API is implemented.
- The current build does not target the proposed Heltec ESP32-S3 SX1262 V3 gateway.

## Client reference

Gateway CI checks node source at `a0e48ba9c1756e655318168d01ffaed10c151ec7` and the historical v1 baseline at `737cfff590c0cbea33895df17c3e3ffc06449e15`.

The ESP32-S3 client `main` has advanced to `ade92292bf945a92c297b1542b944b7ed3cd7ee2` as inspected on 2026-11-10. Its current LoRa framing header still defines Energy Station v1 and the same wire sizes; full gateway regression against that exact commit has **not** been run by the pinned gateway CI. Do not describe the CI-pinned revision as today's client HEAD.

Client PR #5 and gateway PR #2 are merged; they are not outstanding draft PRs.

## Tests and outstanding work

GitHub Actions for gateway commit `8f95c0e` completed successfully on 2026-11-07:
https://github.com/gedxxe/esp32-modbus-energy-gateway/actions/runs/42379551625

This confirms the configured software checks for that commit, not a physical gateway test.

Open work:
1. Test compatibility against the newest client source without changing LoRa v1 wire semantics.
2. Measure ACK backlog with the configured client intervals and actual number of nodes.
3. Validate gateway/node communication, restart handling, archive storage, and radio behavior with physical devices.
4. Plan capacity/recovery for 64 archive records and 64 sequence ledger entries before long-term use.
5. Implement Heltec board support and website uplink only as separately reviewed changes; neither exists in this baseline.

Documentation cleanup is separate from firmware fixes. Do not merge a wording-only change with radio, storage, or board modifications.
