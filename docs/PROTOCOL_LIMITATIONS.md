<p align="left"><img src="assets/midea-brand.svg" alt="Midea" width="185" /></p>

![PlatformIO](assets/badges/platformio.svg) ![Protocol](assets/badges/protocol.svg) ![Board](assets/badges/board.svg)

# Observed protocol v1 limitations (wire format unchanged)

These are source-based reproductions and future coordinated proposals, not physical RF test claims. They do not block this compatible MVP.

| Limitation / reproduction | Current compatible handling | Possible coordinated change |
|---|---|---|
| POLL mode1 for missing day: node returns found=false internally and sends no response | Timeout/unavailable, no empty record | Versioned authenticated not-found/busy response |
| ACK then power loss before gateway durable write | Acceptance differs from durability; rerequest immutable day | Explicit durable receipt in a future version with node retention preserved |
| Lost gateway sequence high-water, node remembers lastDown | Stop POLL, preserve flash, rotate node session locally | Authenticated epoch/reset handshake with replay analysis |
| Recorded authenticated HELLO | Candidate only, fresh correlated POLL, candidate rate limit | Formal challenge-response negotiation for v2 |
| One node pending live/archive request; per-boot 1% gate, CAD and contention | Shared gateway budget, ACK priority, one archive, bounded queues | Negotiated scheduling would need both endpoints; current polling is not TDMA |
| No ID/PHY/RTC/key/reboot commands | Configure node locally, match inventory | Separate replay-safe configuration protocol after LoRa acceptance |
| Remote retry/health details not transmitted | Report observed gateway duplicates/timeouts/CRC/auth, never invent remote metrics | Explicit diagnostic payload if required |

Archive cleanup and detection of a completely erased installation are gateway maintenance limits. See commissioning recovery procedures. HMAC does not encrypt energy values or metadata.

<details><summary><b>Node-side CAD is a medium-access update, not protocol v2</b></summary>

The companion [ESP32-S3 node PR](https://github.com/gedxxe/esp32-modbus-energy/pull/5) uses SX1262 CAD before autonomous HELLO/TELEMETRY/CHECKPOINT, retaining the same signed 24-byte-header LoRa v1 packets and ACK semantics. Gateway-only changes cannot force a hidden transmitter to obey a slot; a future TDMA design must be explicitly versioned, synchronized and tested as a coordinated node/gateway change.

</details>
