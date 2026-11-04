// SPDX-License-Identifier: GPL-3.0-only
#include "checkpoint.h"
#include <cfloat>
namespace gw {
namespace {
double
expected(const Point &x) { // Historical engineering validation only; live kWh is never rescaled.
  if (x.profile == 0)
    return double(x.raw) * .001 * double(x.ptNum) / x.ptDen * x.ct;
  if (x.profile == 1 || x.profile == 5)
    return double(x.raw) * .01 * double(x.ptNum) / x.ptDen * x.ct;
  if (x.profile == 2)
    return double(x.raw) * .1;
  if (x.profile == 3 || x.profile == 4) {
    float f;
    memcpy(&f, &x.raw, 4);
    return !(x.raw & 0x80000000u) && std::isfinite(f) ? double(f) * (x.profile == 4 ? .001 : 1)
                                                      : NAN;
  }
  return NAN;
}
} // namespace
bool decodeRecord(const uint8_t *p, size_t n, Record &out) {
  if (!p || n != 446 || u32(p) != 0x31545043 || u16(p + 4) != 3 || u16(p + 6) != 10 ||
      u32(p + 20) != 0x06001900 || u16(p + 444) != crc16(p, 444))
    return false;
  Record r;
  r.day = u32(p + 8);
  r.revision = u32(p + 12);
  r.generation = u32(p + 16);
  if (!validDay(r.day) || !r.generation)
    return false;
  for (unsigned i = 0; i < 10; i++) {
    const uint8_t *q = p + 24 + i * 42;
    Point &x = r.points[i];
    x.status = q[0];
    if (x.status > 2 || q[39])
      return false;
    if (x.status != 1) {
      for (unsigned j = 1; j < 42; j++)
        if (q[j])
          return false;
      continue;
    }
    x.slave = q[1];
    x.parity = char(q[2]);
    x.direction = q[3];
    x.started = u32(q + 4);
    x.captured = u32(q + 8);
    x.raw = u32(q + 12);
    x.baud = u32(q + 16);
    x.ptNum = u16(q + 20);
    x.ptDen = u16(q + 22);
    x.ct = u16(q + 24);
    x.energy = f64(q + 26);
    x.generation = u32(q + 34);
    x.profile = q[38];
    x.meterId = u16(q + 40);
    if (x.profile >= 6 || !x.slave || x.slave > 247 || !x.baud || x.baud > 1000000 ||
        (x.parity != 'N' && x.parity != 'E' && x.parity != 'O') || x.direction > 1 || !x.ptNum ||
        x.ptNum > 9999 || !x.ptDen || x.ptDen > 9999 || !x.ct || x.ct > 9999 ||
        (x.profile != 0 && x.ptDen != 1) ||
        (x.profile >= 2 && x.profile <= 4 && (x.ptNum != 1 || x.ptDen != 1 || x.ct != 1)))
      return false;
    uint64_t target = uint64_t(r.day) * 86400 + CaptureMinutes[i] * 60 - 25200;
    double e = expected(x);
    if (!validUTC(x.started) || !validUTC(x.captured) || !x.generation ||
        !std::isfinite(x.energy) || x.energy < 0 || !std::isfinite(e) || e < 0 ||
        x.started < target || x.captured < x.started || uint64_t(x.captured) > target + 5 ||
        x.captured - x.started > 2 || std::fabs(x.energy - e) > 16 * DBL_EPSILON * std::fmax(1., e))
      return false;
  }
  out = r;
  return true;
}
void Assembly::start(uint16_t net, uint16_t id, uint64_t sess, uint32_t corr, uint32_t day) {
  *this = Assembly{};
  active = true;
  network = net;
  node = id;
  session = sess;
  correlation = corr;
  requestedDay = day;
}
FragmentResult Assembly::add(uint16_t net, uint16_t id, uint64_t sess, const uint8_t *p, size_t n) {
  if (!active || net != network || id != node || sess != session || n < 13 || n > 112 ||
      u32(p) != correlation || u16(p + 10) != 446)
    return FragmentResult::Rejected;
  uint32_t day = u32(p + 4);
  size_t off = u16(p + 8), len = n - 12;
  if (!validDay(day) || (requestedDay && day != requestedDay) || (actualDay && day != actualDay) ||
      off >= 446 || len > 100 || off + len > 446)
    return FragmentResult::Rejected;
  bool all = true, any = false;
  for (size_t i = 0; i < len; i++) {
    if (seen[off + i]) {
      any = true;
      if (bytes[off + i] != p[12 + i])
        return FragmentResult::Rejected;
    } else
      all = false;
  }
  // Exact byte duplicates are allowed; partial overlap is ambiguous and rejected.
  if (any && !all)
    return FragmentResult::Rejected;
  if (all)
    return FragmentResult::Duplicate;
  actualDay = day;
  for (size_t i = 0; i < len; i++) {
    bytes[off + i] = p[12 + i];
    seen[off + i] = 1;
  }
  count += len;
  if (count != 446)
    return FragmentResult::Partial;
  Record r;
  if (!decodeRecord(bytes, 446, r) || r.day != day)
    return FragmentResult::InvalidRecord;
  complete = true;
  return FragmentResult::Complete;
}
} // namespace gw
