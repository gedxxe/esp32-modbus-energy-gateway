# Gateway-only re-audit: LoRa client compatibility — 2026-11-07

Source baseline: gateway main `bca68cf94c76d36ce8f9ffdac067c50af6f6a358`.
The ESP32-S3 client is strictly **read-only**, inspected at
`a0e48ba9c1756e655318168d01ffaed10c151ec7`.
The original Energy Station v1 reference `737cfff590c0cbea33895df17c3e3ffc06449e15`
remains in legacy cross-peer codec CI.

## Confirmed gateway defects and fixes

- **Current client profile drift:** The node defines eight meter profiles;
  previous gateway decoder rejected TELEMETRY with profile IDs 6 and 7.
  Gateway now recognizes them, rejecting unrecognized profile >=8.
- **Archived Schneider measurements:** 446-byte capture schema 3 is unchanged.
  Profile 6 is PM5100 historical Delivered-only energy; 7 is PM5110 D+R
  active kWh. Both use raw IEEE754 Float32 (not UInt32) and ratio unity.
  Gateway checks the same primary kWh, raw/engineering consistency, zero/positive
  finite values, nonnegative float sign bit, and immutable profile identity;
  never multiply live kWh by CT/PT again.
- **Slow-PHY ACK backlog:** The prior SF12 fix protected one ACK against
  the 1% duty gate, but not a burst of four or more queued ACKs.
  Pending ACKs now retain deadlines based on the full current ACK queue
  and the existing pacing gate. Queued POLL deadlines and their node request
  state are extended together so stale POLLs cannot remain pending when
  already retired by the request state machine.
- **Expired packet safety:** Deadline extension ignores already-expired ACK/POLL
  jobs; an authenticated retransmission cannot coalesce into an expired ACK.
- **CI regression:** Compiles exact client MeterProfiles.h against gateway
  immutable IDs and verifies both Schneider Float32 profiles, in addition to
  existing legacy v1 wire interoperability and gateway native tests.
  Native tests include valid/malformed Schneider checkpoints, invalid CT/NaN,
  SF12 4-node ACK burst and expired-ACK behavior.

## Unchanged contracts

Energy Station v1 LoRa `ES`, little-endian header 24 bytes, max payload
112 bytes, truncated HMAC-SHA256 tag 16 bytes, SX1262, HELLO/ACK/TELEMETRY/
POLL/CHECKPOINT behavior, gateway defaults and node firmware are unchanged.
Gateway does not change the client's 10-minute website uplink policy.
Gateway only sees scaled kWh from node; all Modbus register reading stays
on the node.

## Operational limits and remaining acceptance

The **source code and CI do not prove physical RF performance**.
The fixed-point 1% pacing preserves gateway-airtime accounting, but may
make ACK late relative to node retry lifetime at SF11/12 with many nodes.
CAD is only channel activity detection and cannot prevent hidden-terminal
collisions. The 16-entry gateway inventory is not a proven simultaneous-node
capacity. Bench-test one node, then four, including gateway/node reboot,
flash/NVS interruption, corrupted checkpoints, missing-day recovery,
400 m factory RF path, antenna link budget, energy consistency and
application-session pairing. Check regional RF authorization and EIRP
before using the default 915 MHz on air.
