// SPDX-License-Identifier: GPL-3.0-only
// Schema 3 semantic subset adapted from upstream checkpoint_log.h and MeterProfiles.h.
// No Modbus register map or acquisition application is copied. See NOTICE.
#pragma once
#include "bytes.h"
namespace gw {
constexpr size_t RecordSize = 446, PointCount = 10;
constexpr uint16_t CaptureMinutes[10] = {360, 390, 420, 450, 480, 1020, 1050, 1080, 1110, 1140};
constexpr uint32_t MinUTC = 1704067200u, MaxUTC = 4102444799u;
inline bool validUTC(uint32_t t) {
  return t >= MinUTC && uint64_t(t) + 25200 <= MaxUTC;
}
inline bool validDay(uint32_t d) {
  return d >= MinUTC / 86400 && d <= MaxUTC / 86400;
}
struct Point {
  uint8_t status = 0, slave = 0, direction = 0, profile = 0;
  char parity = 0;
  uint32_t started = 0, captured = 0, raw = 0, baud = 0, generation = 0;
  uint16_t ptNum = 0, ptDen = 0, ct = 0, meterId = 0;
  double energy = 0;
};
struct Record {
  uint32_t day = 0, revision = 0, generation = 0;
  Point points[10]{};
};
bool decodeRecord(const uint8_t *p, size_t n, Record &r);
enum class FragmentResult { Rejected, Duplicate, Partial, Complete, InvalidRecord };
class Assembly {
public:
  bool active = false, complete = false;
  uint16_t network = 0, node = 0;
  uint64_t session = 0;
  uint32_t correlation = 0, requestedDay = 0, actualDay = 0;
  size_t count = 0;
  uint8_t bytes[446]{}, seen[446]{};
  void start(uint16_t net, uint16_t id, uint64_t sess, uint32_t corr, uint32_t day);
  FragmentResult add(uint16_t net, uint16_t id, uint64_t sess, const uint8_t *p, size_t n);
};
} // namespace gw
