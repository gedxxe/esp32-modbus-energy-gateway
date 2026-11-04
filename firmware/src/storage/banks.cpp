// SPDX-License-Identifier: GPL-3.0-only
#include "banks.h"
namespace gw {
uint8_t Banks::bytes[MaxBlob]{};
uint8_t Banks::verify[MaxBlob]{};
bool Banks::valid(const uint8_t *p, size_t n) const {
  return n >= 20 && n <= MaxBlob && !memcmp(p, "GWB1", 4) && p[4] == 1 && p[5] == kind && !p[6] &&
         !p[7] && u32(p + 8) && u32(p + 12) == n - 20 && u32(p + n - 4) == crc32(p, n - 4);
}
Load Banks::load(uint8_t *out, size_t n, bool strict) {
  bank = -1;
  generation = 0;
  bool any = false, bad = false;
  for (unsigned i = 0; i < 2; i++) {
    const char *key = i ? b : a;
    size_t z = backend.size(key);
    if (!z)
      continue;
    any = true;
    if (z != n + 20 || z > MaxBlob || backend.read(key, bytes, z) != z || !valid(bytes, z)) {
      bad = true;
      continue;
    }
    uint32_t g = u32(bytes + 8);
    if (bank < 0 || forward(g, generation)) {
      memcpy(out, bytes + 16, n);
      generation = g;
      bank = int(i);
    }
  }
  return (strict && bad) || (any && bank < 0) ? Load::Corrupt : bank < 0 ? Load::Empty : Load::Ok;
}
bool Banks::save(const uint8_t *p, size_t n) {
  if (n + 20 > MaxBlob || generation == UINT32_MAX)
    return false;
  memset(bytes, 0, 16);
  memcpy(bytes, "GWB1", 4);
  bytes[4] = 1;
  bytes[5] = kind;
  p32(bytes + 8, generation + 1);
  p32(bytes + 12, uint32_t(n));
  memcpy(bytes + 16, p, n);
  p32(bytes + 16 + n, crc32(bytes, n + 16));
  size_t z = n + 20;
  const char *key = bank == 0 ? b : a;
  if (backend.write(key, bytes, z) != z || backend.size(key) != z ||
      backend.read(key, verify, z) != z || !equal(bytes, verify, z) || !valid(verify, z))
    return false;
  bank = bank == 0 ? 1 : 0;
  generation++;
  return true;
}
} // namespace gw
