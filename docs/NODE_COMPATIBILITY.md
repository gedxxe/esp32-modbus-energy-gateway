# Node compatibility record

## Audited baseline

Read-only upstream: [gedxxe/esp32-modbus-energy](https://github.com/gedxxe/esp32-modbus-energy), main commit **770512260ef5133296790ee21d2635c55f00196f**, node firmware **3.5.0**, and **Energy Station radio protocol v1**. The gateway implements the existing wire contract; firmware and radio protocol versions are separate. The node repository is not changed by this work.

The implementation was cross-checked against upstream `docs/LORA_PROTOCOL.md`, `firmware/include/lora_protocol.h`, `firmware/src/lora_service.cpp`, `firmware/include/checkpoint_log.h` and the public interoperability vector. `docs/reference-files.json` still retains hashes from the earlier 3.5.0 baseline for reproducibility and is not presented as an updated hash manifest. The wire layout and field meanings used by the gateway remain the protocol v1 contract.

## Wire contract implemented

Explicit LE fields, `ES`, version1, 24-byte header, payload≤112, tag16, packet≤152; key32; first16 bytes HMAC-SHA256(header+payload), constant-time verification. Types1–5 match. HELLO24/schema3/caps/version string, ACK1 with swapped addresses and echoed node session/sequence/type, telemetry32, POLL5 modes0/1, checkpoint fragment12+1..100 with total446 are implemented in production `core/protocol.*`.

Default PHY: 915 MHz / BW125 kHz / SF7 / CR4/5 / preamble8 / logical sync0x12, explicit header/normal IQ/physical CRC. Source validation ranges: carrier803–930 MHz, BW125/250/500, SF7–12, CR5–8, preamble8–64, TX0–22 dBm, syncbyte0–255. RadioLib7.8.1 maps logical0x12 to SX126x register0x1424; gateway passes the logical byte. Disabled by default; no fixture key in runtime config.

## Checkpoint schema and provenance

Schema3 = **446 bytes**: header24 (`CPT1` LE magic0x31545043, schema3, pointcount10, WIB day, revision, generation, marker0x06001900), ten42-byte points, trailing LE Modbus-polynomial CRC16. Point status, slave/parity/direction, start/read UTC, raw counter, baud, PT numerator/denominator, CT, engineering binary64 kWh, generation, immutable profile ID, reserved byte and saved meter ID are preserved. Missed/waiting points have zero fields and absent energy semantics.

Semantic validation follows upstream `decodeCaptureDay`: day/clock2024–2099 bounds, source profile/slave/baud/parity/direction/ratios, capture window+5s, sample duration≤2s, generations, reserved fields, finite nonnegative energy and historical profile/raw consistency within16×DBL_EPSILON. Profile IDs0..5 retain their immutable meanings. `checkpoint.cpp` extracts only schema decoding/historical validation facts and arithmetic from `checkpoint_log.h`, `MeterProfiles.h`, `clock_guard.h` and CRC16 from `energy.h`; no node tracker, Modbus register map or meter application was copied. Live telemetry is never multiplied by PT/CT again.

Gateway code is GPL-3.0-only, preserving upstream SPDX/provenance in NOTICE/LICENSE. Header byte primitives/layout derive from `lora_protocol.h`. Test vector and independent Python checker are copied unmodified. Other gateway code is independently implemented; third-party UI/dependency licenses are retained separately.

## Findings resolved before implementation

1. Source/documents agree on packet layout, schema3 and fragment sizes. The public vector's HELLO firmware string is **3.4.0**, although the current firmware is 3.5.0. This is a valid earlier fixture for unchanged protocol1; do not rewrite its tag/version string. Production codec accepts and roundtrips the exact bytes.
2. Documentation names a future C6 gateway; the owner explicitly selected C3. That changes board/build choices and no wire bytes.
3. `checkpoint_log.h` also decodes schema1/2 locally. Radio source freezes/reserializes schema3, so gateway requires446/schema3; it does not import the entire legacy storage application.
4. Node `receivedPacket()` accepts forward POLL only when paired and queues live/archive work; it does not ACK POLL. Gateway waits for correlated telemetry/fragments. A late/foreign ACK cannot finish a request. HELLO ACK must precede a successful challenge.
5. Node's10s default ACK timeout is not a POLL deadline. Contention can reach30s plus address offset; the shared1% gate can add longer waits. Gateway includes contention/duty/owner-service margins and prioritizes periodic uplinks.
6. Missing-day archive lookup produces no fragments, not an explicit not-found reply. Gateway reports timeout/unavailable, not an empty durable archive.

## Compatibility evidence

- `python tools/test_native.py`: **580 production-core assertions PASS**, plus115 independent HMAC comparisons: empty deployment defaults, three identity/key pairs, foreign/disabled/HMAC rejection, malformed bytes, duplicates/delayed ACK, replay HELLO/POLL, sequence reservations across reboot/corruption/full table, interleaved same/other-node telemetry, unordered/missing/duplicate/conflicting/corrupt fragments, full TX queue/expiry/fairness, absent-module lifecycle, inventory recovery and interrupted writes. Release0.1.1 adds per-node packet observation fields to HTTP only; duplicate packets do not refresh telemetry age, and wire/checkpoint formats are unchanged.
- Same production decode/encode/HMAC compiled natively roundtrips the unchanged public vector exactly. Copied upstream Python checker independently verifies its SHA256-HMAC. **PASS**.
- Final target compilation, asset QA and counts are recorded in `VERIFICATION.md`; RF acceptance remains **AWAITING_HARDWARE**.

## Limitations

See `PROTOCOL_LIMITATIONS.md` for source reproductions and possible coordinated future changes. No gateway-only remote ID/reset command, encryption, time setter, TDMA or status extension was added to v1. No node fixes were made silently.
