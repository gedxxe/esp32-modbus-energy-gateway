// SPDX-License-Identifier: GPL-3.0-only
// LE header contract derived from esp32-modbus-energy/lora_protocol.h; see NOTICE.
#pragma once
#include "crypto.h"
namespace gw {
constexpr size_t HeaderSize = 24, TagSize = 16, MaxPayload = 112, MaxPacket = 152;
enum Type : uint8_t { Hello = 1, Ack = 2, Telemetry = 3, Poll = 4, Checkpoint = 5 };
enum class Reject : uint8_t {
  None,
  Length,
  Header,
  Address,
  Unknown,
  Disabled,
  Auth,
  Payload,
  Session,
  Replay,
  Correlation,
  Queue,
  Storage
};
const char *reason(Reject r);
struct Frame {
  uint8_t type = 0, length = 0;
  uint16_t network = 0, source = 0, destination = 0;
  uint32_t sequence = 0;
  uint64_t session = 0;
  uint8_t payload[112]{};
};
Reject inspect(const uint8_t *p, size_t n); // only unauthenticated bounded routing metadata
Reject decode(const uint8_t *p, size_t n, const uint8_t key[32], Frame &f);
size_t encode(const Frame &f, const uint8_t key[32], uint8_t out[MaxPacket]);
bool validPayload(const Frame &f);
Frame acknowledge(const Frame &f);
struct Live {
  uint32_t correlation = 0, utc = 0, sampleAge = 0, day = 0, revision = 0;
  uint16_t meterId = 0;
  uint8_t flags = 0, profile = 0;
  double kwh = 0;
};
bool decodeLive(const Frame &f, Live &live);
} // namespace gw
