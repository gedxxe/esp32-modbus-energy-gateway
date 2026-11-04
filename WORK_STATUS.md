# Gateway development status

## Baseline and scope

Gateway work is based on `gedxxe/esp32-modbus-energy-gateway`, with the upstream `gedxxe/esp32-modbus-energy` firmware used only as a read-only protocol reference. The upstream node at main commit `770512260ef5133296790ee21d2635c55f00196f` uses Energy Station radio protocol v1 and firmware version 3.5.0. The gateway remains a separate ESP32-C3 / SX1262 application. No changes to the node's Modbus register reading, operating settings or public recovery AP password are part of this gateway work.

## Implemented in source

The gateway has a fixed-length authenticated packet decoder, per-node HMAC keys, replay suppression, verified downlink sequence reservation, a radio task owning SPI/RadioLib, a local commissioning API and dashboard, checkpoint reassembly and archive storage. Manual refresh sends LoRa POLL mode 0 and receives a correlated TELEMETRY response. Current work also distinguishes the last valid kWh reading from the newest telemetry state, and introduces a conservative round-robin refresh policy for nodes that have missed their expected periodic report. The node still controls its own scheduled transmit interval.

## Pending evidence and follow-up

Run the updated UI build/tests, native tests, source audit and ESP32-C3 compilation against the new gateway branch. Confirm the actual generated dashboard bytes are embedded in firmware before flashing. Software tests cannot prove the selected antenna, radio range in the factory or reliability with all four installed nodes; bench commissioning with the source node is still required. Other measurements besides scaled kWh are not available in the current live LoRa v1 payload and must not be claimed on the dashboard.

Keep personal paths, authentication material, compiled ELF files, local cache directories and real network configuration out of source control. Historic commits containing earlier local paths remain accessible until history is deliberately rewritten and all clones, forks and references are handled. The path exposure is a privacy issue even where no password was involved. Architecture detail and physical acceptance procedures are recorded in `docs/RADIO_DESIGN.md` and `docs/COMMISSIONING.md`.
