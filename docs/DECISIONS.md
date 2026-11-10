<p align="left"><img src="assets/midea-brand.svg" alt="Midea" width="185" /></p>

![PlatformIO](assets/badges/platformio.svg) ![Protocol](assets/badges/protocol.svg) ![Board](assets/badges/board.svg)

# Architecture decisions

## D001 — First target and scope

ESP32-C3 supersedes the initial brief's C6 target by explicit user correction. Use its single application core without S3 core affinity assumptions. Board-specific GPIO and module electrical options remain isolated. Implement raw LoRa v1 and local AP commissioning; postpone all backend features.

## D002 — Protocol and evidence

At the time of the initial review, the node reference was `737cfff590c0cbea33895df17c3e3ffc06449e15` (firmware 3.5.0). It is a historical compatibility baseline, not the latest node `main`. Keep the public v1 test vector unchanged and test new client revisions separately. Software checks do not replace physical radio tests.

## D003 — Local UI

Use React with bundled local compressed assets and a bounded HTTP command queue. No CDN, animations library, remote fonts, or external services on the device.

## D004 — Storage isolation and conservative replay recovery

Use a dedicated `gw_nvs` partition for config/sequence banks so framework Wi-Fi NVS recovery does not erase gateway state. Config falls back to a verified bank; a malformed existing sequence bank fails closed. Reserve 64 sequences before TX, retain at most 64 session identities without silent eviction. Full/corrupt/lost state needs local recovery and actual node session rotation; no invented remote reset.

## D005 — Archive preservation and bounded retention

Use 64 append-only CRC-protected schema3 records in LittleFS, exact source identity and all ten capture points retained. No automatic mount formatting, record overwriting or identity merging. A full/corrupt store visibly refuses new durable writes. Live telemetry stays RAM-only to avoid per-packet flash writes; packet ACK never promises durability.

## D006 — Single C3 owner tasks

Use supplied FreeRTOS, unpinned radio/storage workers on the single application CPU. Only radio owns SPI; storage owns all flash I/O. HTTP uses queues/key-free snapshots. Radio reset/BUSY/TX/recovery deadlines are tested through the same portable lifecycle used by production; driver calls remain bounded by RadioLib's100ms BUSY timeout.

## D007 — Commissioning access

Fresh AP password and a separate HTTP access code per boot, generated after Wi-Fi starts for hardware RNG entropy. Require the custom code header for all data/commands and keep credentials in browser RAM only. NVS at-rest encryption is not configured. No deployed credentials are baked into the binary. See Espressif's [RNG prerequisites](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32c3/api-reference/system/random.html).

## D008 — Guard custom factory upload layout

Inspection of actual build output showed pinned pioarduino55.03.311 selects the application offset only for ota_0 and unconditionally adds legacy boot_app0 at0xE000. Our factory app is0x20000 and gw_nvs starts0xE000. A project-local post script removes that legacy image, enforces factory offset/capacity for size/bin/upload actions, and emits a verified layout record. No global SDK patch and no wire change. Only esptool upload is supported. Do not use generated combined factory images to update an installation: filling their gaps can erase persistent partitions. Release supplies separate images and exact offsets.

## D009 — Per-ID cards and honest link observations

UI0.1.1 follows the owner's card-grid sketch, using the existing16-entry NVS inventory instead of adding another store. New IDs get an unused address suggestion; form saves keep the revision captured at opening to reject stale edits. Per-node last authenticated packet/type/sequence, unique accepted and duplicate counters are additive key-free HTTP diagnostics, with no radio or storage format changes. A duplicate updates RF observation age but never refreshes measurement time. Data freshness, candidate/confirmed sessions and signal observations remain separate; no invented packet-loss score or fixed offline timeout is inferred from unknown node intervals. One shared status request refreshes all cards; browser monotonic age updates add no radio traffic or flash writes.

## D010 — Standard VS Code PlatformIO workspace

The ESP32-C3 project root is `firmware/`, not the Git repository root. PlatformIO's standard `src/` and `include/` folders contain runtime code and headers; `firmware/.vscode/` stores project settings. Documentation, Python host validation, React dashboard and package tooling remain in separate repository-root folders. This matches the physical workflow used by the ESP32-S3 meter node.

## D011 — Same v1 protocol, dual-endpoint CAD

A separately reviewed ESP32-S3 node branch adds nonblocking SX1262 CAD and randomized backoff before uplinks. The gateway already performs CAD before discretionary POLLs, preserves downlink sequence identity across deferrals, and reserves TX queue slots for ACKs. We keep Energy Station v1 framing and 915 MHz software defaults unchanged. CAD cannot eliminate hidden-terminal collisions or provide deterministic slots without a new joint time-synchronization protocol.

## Baseline note — 2026-11-10

The gateway CI currently pins node `a0e48ba9c1756e655318168d01ffaed10c151ec7`, while the inspected client `main` is `ade92292bf945a92c297b1542b944b7ed3cd7ee2`. The current gateway remains an ESP32-C3 with a local Wi-Fi AP. Future Heltec ESP32-S3 gateway support or a Wi-Fi/backend uplink requires a separate implementation and tests; neither is part of these historical decisions.
