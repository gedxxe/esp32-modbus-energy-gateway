// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cmath>
#include <limits>
namespace gw {
inline uint16_t u16(const uint8_t *p) {
  return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}
inline uint32_t u32(const uint8_t *p) {
  uint32_t n = 0;
  for (unsigned i = 0; i < 4; i++)
    n |= uint32_t(p[i]) << (8 * i);
  return n;
}
inline uint64_t u64(const uint8_t *p) {
  uint64_t n = 0;
  for (unsigned i = 0; i < 8; i++)
    n |= uint64_t(p[i]) << (8 * i);
  return n;
}
inline void p16(uint8_t *p, uint16_t n) {
  for (unsigned i = 0; i < 2; i++)
    p[i] = uint8_t(n >> (8 * i));
}
inline void p32(uint8_t *p, uint32_t n) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = uint8_t(n >> (8 * i));
}
inline void p64(uint8_t *p, uint64_t n) {
  for (unsigned i = 0; i < 8; i++)
    p[i] = uint8_t(n >> (8 * i));
}
inline double f64(const uint8_t *p) {
  static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559, "IEEE754 required");
  uint64_t n = u64(p);
  double d;
  memcpy(&d, &n, 8);
  return d;
}
inline uint16_t crc16(const uint8_t *p, size_t n) {
  uint16_t c = 0xffff;
  while (n--) {
    c ^= *p++;
    for (unsigned i = 0; i < 8; i++)
      c = (c >> 1) ^ ((c & 1) ? 0xa001 : 0);
  }
  return c;
}
inline uint32_t crc32(const uint8_t *p, size_t n) {
  uint32_t c = ~0u;
  while (n--) {
    c ^= *p++;
    for (unsigned i = 0; i < 8; i++)
      c = (c >> 1) ^ ((c & 1) ? 0xedb88320u : 0);
  }
  return ~c;
}
inline bool equal(const uint8_t *a, const uint8_t *b, size_t n) {
  uint8_t d = 0;
  for (size_t i = 0; i < n; i++)
    d |= a[i] ^ b[i];
  return d == 0;
}
inline bool forward(uint32_t n, uint32_t old) {
  return n != old && uint32_t(n - old) < 0x80000000u;
}
inline void wipe(void *p, size_t n) {
  volatile uint8_t *q = static_cast<volatile uint8_t *>(p);
  while (n--)
    *q++ = 0;
}
} // namespace gw
