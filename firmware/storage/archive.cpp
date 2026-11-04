// SPDX-License-Identifier: GPL-3.0-only
#include "archive.h"
#include <cstdio>
namespace gw {
void Archives::keys(unsigned i, char a[12], char b[12]) const {
  snprintf(a, 12, "r%02ua", i);
  snprintf(b, 12, "r%02ub", i);
}
bool Archives::load() {
  count = 0;
  errors = 0;
  for (unsigned i = 0; i < ArchiveCapacity; i++) {
    entries[i] = ArchiveInfo{};
    char a[12], b[12];
    keys(i, a, b);
    Banks bank(backend, a, b, 3);
    Load r = bank.load(bytes, sizeof(bytes));
    if (r == Load::Empty)
      continue;
    Record record;
    if (r != Load::Ok || !decodeRecord(bytes + 48, 446, record) || !u16(bytes) || !u16(bytes + 2) ||
        u16(bytes + 2) == 65535) {
      errors++;
      continue;
    }
    ArchiveInfo &e = entries[i];
    e.used = true;
    e.network = u16(bytes);
    e.node = u16(bytes + 2);
    e.boot = u64(bytes + 8);
    e.received = u64(bytes + 16);
    e.session = u64(bytes + 24);
    e.correlation = u32(bytes + 32);
    e.day = record.day;
    e.revision = record.revision;
    count++;
  }
  healthy = errors == 0;
  return healthy;
}
bool Archives::save(uint16_t net, uint16_t node, uint64_t sess, uint32_t corr,
                    const uint8_t *record, uint64_t boot, uint64_t received) {
  Record r;
  if (!healthy || !decodeRecord(record, 446, r))
    return false;
  int slot = -1;
  for (unsigned i = 0; i < ArchiveCapacity; i++) {
    const ArchiveInfo &e = entries[i];
    if (!e.used && slot < 0)
      slot = int(i);
    if (e.used && e.network == net && e.node == node && e.day == r.day &&
        e.revision == r.revision) {
      uint8_t old[ArchiveBytes];
      if (!read(i, old))
        return false;
      return equal(old + 48, record, 446);
    }
  }
  if (slot < 0)
    return false;
  memset(bytes, 0, sizeof(bytes));
  p16(bytes, net);
  p16(bytes + 2, node);
  p64(bytes + 8, boot);
  p64(bytes + 16, received);
  p64(bytes + 24, sess);
  p32(bytes + 32, corr);
  memcpy(bytes + 48, record, 446);
  char a[12], b[12];
  keys(unsigned(slot), a, b);
  Banks bank(backend, a, b, 3);
  uint8_t old[ArchiveBytes];
  if (bank.load(old, sizeof(old)) != Load::Empty)
    return false;
  if (!bank.save(bytes, sizeof(bytes)))
    return false;
  ArchiveInfo &e = entries[slot];
  e.used = true;
  e.network = net;
  e.node = node;
  e.session = sess;
  e.correlation = corr;
  e.boot = boot;
  e.received = received;
  e.day = r.day;
  e.revision = r.revision;
  count++;
  return true;
}
bool Archives::read(unsigned i, uint8_t *out) {
  if (i >= ArchiveCapacity || !entries[i].used)
    return false;
  char a[12], b[12];
  keys(i, a, b);
  Banks bank(backend, a, b, 3);
  return bank.load(out, ArchiveBytes) == Load::Ok;
}
} // namespace gw
