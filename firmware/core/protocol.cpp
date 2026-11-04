// SPDX-License-Identifier: GPL-3.0-only
#include "protocol.h"
namespace gw {
const char *reason(Reject r) {
  static const char *const names[] = {
      "accepted",      "length",         "header",  "address", "unknown_node",
      "disabled_node", "authentication", "payload", "session", "replay",
      "correlation",   "queue_full",     "storage"};
  return names[unsigned(r)];
}
Reject inspect(const uint8_t *p, size_t n) {
  if (!p || n < 40 || n > MaxPacket || p[10] > 112 || n != size_t(40 + p[10]))
    return Reject::Length;
  if (p[0] != 'E' || p[1] != 'S' || p[2] != 1 || p[3] < 1 || p[3] > 5 || p[11] || !u32(p + 12) ||
      !u64(p + 16))
    return Reject::Header;
  if (!u16(p + 4) || !u16(p + 6) || u16(p + 6) == 65535 || !u16(p + 8) || u16(p + 8) == 65535 ||
      u16(p + 6) == u16(p + 8))
    return Reject::Address;
  return Reject::None;
}
bool validPayload(const Frame &f) {
  switch (f.type) {
  case Hello: {
    if (f.length != 24 || f.payload[0] != 3 || (f.payload[1] & ~7) || !(f.payload[1] & 4))
      return false;
    bool zero = false;
    for (unsigned i = 2; i < 24; i++) {
      uint8_t c = f.payload[i];
      if (!c)
        zero = true;
      else if (zero || c < 32 || c > 126)
        return false;
    }
    return zero && f.payload[2];
  }
  case Ack:
    return f.length == 1 &&
           (f.payload[0] == Hello || f.payload[0] == Telemetry || f.payload[0] == Checkpoint);
  case Telemetry: {
    if (f.length != 32 || (f.payload[12] & ~31) || f.payload[13] >= 6)
      return false;
    if ((f.payload[12] & 2) &&
        (u32(f.payload + 4) < 1704067200u || uint64_t(u32(f.payload + 4)) + 25200 > 4102444799u))
      return false;
    if (u32(f.payload + 16) &&
        (u32(f.payload + 16) < 1704067200u / 86400 || u32(f.payload + 16) > 4102444799u / 86400))
      return false;
    if (f.payload[12] & 1) {
      double v = f64(f.payload + 24);
      if (!std::isfinite(v) || v < 0 || u32(f.payload + 8) > 15000)
        return false;
    }
    return true;
  }
  case Poll:
    return f.length == 5 && f.payload[0] <= 1;
  case Checkpoint:
    return f.length >= 13 && f.length <= 112 && u16(f.payload + 10) == 446 &&
           u16(f.payload + 8) < 446 && size_t(u16(f.payload + 8)) + f.length - 12 <= 446 &&
           u32(f.payload);
  default:
    return false;
  }
}
Reject decode(const uint8_t *p, size_t n, const uint8_t key[32], Frame &f) {
  Reject r = inspect(p, n);
  if (r != Reject::None)
    return r;
  uint8_t mac[32];
  hmac(key, p, n - 16, mac);
  bool ok = equal(mac, p + n - 16, 16);
  wipe(mac, 32);
  if (!ok)
    return Reject::Auth;
  Frame x;
  x.type = p[3];
  x.length = p[10];
  x.network = u16(p + 4);
  x.source = u16(p + 6);
  x.destination = u16(p + 8);
  x.sequence = u32(p + 12);
  x.session = u64(p + 16);
  memcpy(x.payload, p + 24, x.length);
  if (!validPayload(x))
    return Reject::Payload;
  f = x;
  return Reject::None;
}
size_t encode(const Frame &f, const uint8_t key[32], uint8_t *p) {
  if (f.length > 112 || !f.network || !f.source || f.source == 65535 || !f.destination ||
      f.destination == 65535 || f.source == f.destination || !f.session || !f.sequence ||
      !validPayload(f))
    return 0;
  p[0] = 'E';
  p[1] = 'S';
  p[2] = 1;
  p[3] = f.type;
  p16(p + 4, f.network);
  p16(p + 6, f.source);
  p16(p + 8, f.destination);
  p[10] = f.length;
  p[11] = 0;
  p32(p + 12, f.sequence);
  p64(p + 16, f.session);
  memcpy(p + 24, f.payload, f.length);
  uint8_t mac[32];
  hmac(key, p, 24 + f.length, mac);
  memcpy(p + 24 + f.length, mac, 16);
  wipe(mac, 32);
  return 40 + f.length;
}
Frame acknowledge(const Frame &f) {
  Frame a = f;
  a.type = Ack;
  a.source = f.destination;
  a.destination = f.source;
  a.length = 1;
  memset(a.payload, 0, 112);
  a.payload[0] = f.type;
  return a;
}
bool decodeLive(const Frame &f, Live &x) {
  if (f.type != Telemetry || !validPayload(f))
    return false;
  x.correlation = u32(f.payload);
  x.utc = u32(f.payload + 4);
  x.sampleAge = u32(f.payload + 8);
  x.flags = f.payload[12];
  x.profile = f.payload[13];
  x.meterId = u16(f.payload + 14);
  x.day = u32(f.payload + 16);
  x.revision = u32(f.payload + 20);
  x.kwh = (x.flags & 1) ? f64(f.payload + 24) : NAN;
  return true;
}
} // namespace gw
