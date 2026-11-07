# LoRa peer compatibility re-audit — 2026-11-06

Scope: **gateway repository only**. The ESP32-S3 client repository was read but not modified. This is a software-level audit, not an over-the-air factory acceptance report.

## Pinned inputs

- Gateway `main` before audit: `9a5a8829af6b11fd741a92c3e58ccc5d077ca82e`.
- Read-only node `main`: `3896a8b46bd2cd63f9756a46f0b4198a20aac6c2`; firmware version 3.5.0.
- Read-only original LoRa v1 baseline: `737cfff590c0cbea33895df17c3e3ffc06449e15`.
- Both builds pin pioarduino `55.03.311`, RadioLib `7.8.1`, ArduinoJson `7.3.1`.
- Node `lora_protocol.h` / `lora_service.cpp` and gateway `core/protocol.*` / `core/gateway.cpp` / `radio/service.cpp` were compared.

## Wire and default radio settings

| Item | Client (reference) | Gateway |
| --- | --- | --- |
| Modulation | SX1262, LoRa peer-to-peer | SX1262, LoRa peer-to-peer |
| Default radio | 915 MHz; BW125; SF7; CR4/5; preamble8; sync0x12; TX14 dBm | Same |
| Physical framing | Explicit header, normal IQ, CRC2 | Same |
| Frame | `ES` + v1, 24-byte LE header, payload 0–112, HMAC-SHA256 first16 bytes | Same |
| ID/session | Network1, node2 -> gateway1; random session | Gateway1; provision matching network/node/key |
| HELLO / ACK | HELLO type1 / ACK type2 with same sequence/session | Gateway ACK echoes node's sequence, session and type |
| POLL mode0 | Accept forward authorized POLL and respond with correlated TELEMETRY; no POLL ACK | Sends type4 POLL after durable sequence reservation; awaits type3 TELEMETRY |
| Archive | 446-byte schema3, up to 100 data bytes per CHECKPOINT fragment | Validates five fragments, preserves record and ACKs |
| Data | Client scales energy to kWh and publishes validity/age | Does not rescale PT/CT; retains last valid kWh separately |

The client now performs CAD before uplinks and can accept a matching late ACK while the same immutable frame is outstanding. Gateway CAD before discretionary POLLs and direct ACK scheduling remain wire-compatible. CAD is **not** TDMA or hidden-node protection.

## Findings and gateway-only corrections

1. **Slow PHY POLL expiry (corrected):** A fixed 120-second POLL deadline could expire before the 1% duty gate opens following HELLO ACK at SF12/BW125. The queued deadline now incorporates the already-known duty gate, one ACK airtime allowance and a bounded margin. No frame bytes or uplink scheduling in the node changed.
2. **Slow PHY queued ACK expiry (corrected):** A fixed 60-second gateway ACK TTL could expire while TX is prohibited by the previous frame's duty wait. Gateway now retains that queued ACK until the existing duty gate plus a short margin. The node already accepts a late matching ACK while its frame remains outstanding.
3. **Stale interoperability pin (corrected):** CI tested the old baseline and an older companion revision, not current client `main`. CI now tests both the pinned 2026-11-06 current main and original v1 baseline read-only. No checkout has write credentials.
4. **Missing cross-peer CHECKPOINT codec coverage (corrected):** The native cross-peer test now checks the maximum 112-byte checkpoint fragment and its ACK in addition to HELLO/ACK/POLL/TELEMETRY.
5. **Remaining physical limitation:** An operator can choose SF11/SF12 even though the client default ACK timeout is only 10 seconds and the gateway follows a shared 1% TX pacing budget. Delayed ACKs and long queueing are possible; success depends on the client's current in-flight state, node retry/backoff, number of nodes and RF reception. The fixes prevent **premature local queue expiry**, not guaranteed ACK delivery or pairing within any fixed time.

## Preconditions for commissioning

- Match both endpoints' **frequency, bandwidth, spreading factor, coding rate, preamble, logical sync word**, network ID, gateway ID, node ID and the per-node 32-byte key. Gateway disabled by default; node radio is also disabled by default.
- Use one node first. Observe signed HELLO -> gateway HELLO ACK -> correlated POLL -> TELEMETRY and a confirmed session before testing archive transfer. Distinguish `radio initialized`, `candidate`, `confirmed` and `valid kWh`.
- For scale testing use source defaults first (SF7/BW125). At slow PHYs, allow ample airtime and inspect both nodes' ACK/duty counters. The dashboard is local gateway monitoring; LoRa v1 does not change the separately implemented website upload interval.
- Test 4 nodes with distinct keys and addresses, a session reset, node/gateway brownout, missing checkpoint day, local PHY mismatch and radio-offline state.
- Test 400 m factory propagation/antenna/link margin with the actual SX1262 modules and installation route. Source inspection cannot certify range, RF legality, error rate or hardware compatibility.
- Re-run gateway CI / native C++ tests and the ESP32-C3 build on this PR. Do not flash production or merge solely because a codec test passes.

## Relevant gateway files

`firmware/src/core/gateway.cpp`, `tests/native.cpp`, `tests/ptmp_interop.cpp`, `.github/workflows/ci.yml`, `docs/upstream-package-lock.json`.

**No changes were requested or made in `gedxxe/esp32-modbus-energy`.**
