// SPDX-License-Identifier: GPL-3.0-only
#include "sequences.h"
namespace gw {
bool Sequences::persist() {
  memset(wire, 0, sizeof(wire));
  for (size_t i = 0; i < used; i++) {
    uint8_t *p = wire + i * 16;
    p16(p, entries[i].network);
    p16(p + 2, entries[i].node);
    p64(p + 4, entries[i].session);
    p32(p + 12, entries[i].high);
  }
  return banks.save(wire, sizeof(wire));
}
bool Sequences::load(bool fresh) {
  used = 0;
  healthy = false;
  Load r = banks.load(wire, sizeof(wire), true);
  if (r == Load::Corrupt)
    return false;
  if (r == Load::Empty) {
    if (!fresh)
      return false;
    healthy = persist();
    return healthy;
  }
  bool gap = false;
  for (size_t i = 0; i < SessionCapacity; i++) {
    const uint8_t *p = wire + i * 16;
    if (!u64(p + 4)) {
      for (unsigned j = 0; j < 16; j++)
        if (p[j])
          return false;
      gap = true;
      continue;
    }
    if (gap || !u16(p) || !u16(p + 2) || u16(p + 2) == 65535 || u32(p + 12) > 0x7fffff00u)
      return false;
    for (size_t j = 0; j < used; j++)
      if (entries[j].network == u16(p) && entries[j].node == u16(p + 2) &&
          entries[j].session == u64(p + 4))
        return false;
    Reservation &e = entries[used++];
    e.network = u16(p);
    e.node = u16(p + 2);
    e.session = u64(p + 4);
    e.high = u32(p + 12);
    e.next = e.high + 1;
  }
  healthy = true;
  return true;
}
bool Sequences::take(uint16_t net, uint16_t node, uint64_t session, uint32_t &seq) {
  if (!healthy || !net || !node || node == 65535 || !session)
    return false;
  size_t i = 0;
  for (; i < used; i++)
    if (entries[i].network == net && entries[i].node == node && entries[i].session == session)
      break;
  if (i == used) {
    if (used == SessionCapacity)
      return false;
    Reservation &e = entries[used++];
    e = Reservation{};
    e.network = net;
    e.node = node;
    e.session = session;
    e.next = 1;
  }
  Reservation &e = entries[i];
  if (e.next > e.high) {
    if (e.high > 0x7fffff00u - SequenceBlock)
      return false;
    uint32_t high = e.high;
    e.high += SequenceBlock;
    if (!persist()) {
      e.high = high;
      healthy = false;
      return false;
    }
  }
  seq = e.next++;
  return true;
}
} // namespace gw
