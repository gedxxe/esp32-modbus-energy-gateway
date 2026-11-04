// SPDX-License-Identifier: GPL-3.0-only
// Portable SHA-256 (FIPS 180-4) and fixed-32-byte-key HMAC (RFC 2104).
// Same implementation is compiled into firmware and native regression runner.
#include "crypto.h"
namespace gw {
namespace {
constexpr uint32_t k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
uint32_t r(uint32_t n, unsigned b) {
  return (n >> b) | (n << (32 - b));
}
struct Sha {
  uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  uint8_t buf[64]{};
  size_t used = 0;
  uint64_t len = 0;
  void block(const uint8_t *p) {
    uint32_t w[64];
    for (unsigned i = 0; i < 16; i++)
      w[i] = (uint32_t(p[4 * i]) << 24) | (uint32_t(p[4 * i + 1]) << 16) |
             (uint32_t(p[4 * i + 2]) << 8) | p[4 * i + 3];
    for (unsigned i = 16; i < 64; i++)
      w[i] = w[i - 16] + (r(w[i - 15], 7) ^ r(w[i - 15], 18) ^ (w[i - 15] >> 3)) + w[i - 7] +
             (r(w[i - 2], 17) ^ r(w[i - 2], 19) ^ (w[i - 2] >> 10));
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], v = h[7];
    for (unsigned i = 0; i < 64; i++) {
      uint32_t t = v + (r(e, 6) ^ r(e, 11) ^ r(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
      uint32_t t2 = (r(a, 2) ^ r(a, 13) ^ r(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
      v = g;
      g = f;
      f = e;
      e = d + t;
      d = c;
      c = b;
      b = a;
      a = t + t2;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += v;
    wipe(w, sizeof(w));
  }
  void add(const uint8_t *p, size_t n) {
    len += n;
    while (n) {
      size_t z = n < 64 - used ? n : 64 - used;
      memcpy(buf + used, p, z);
      p += z;
      n -= z;
      used += z;
      if (used == 64) {
        block(buf);
        used = 0;
      }
    }
  }
  void end(uint8_t *out) {
    uint64_t bits = len * 8;
    buf[used++] = 0x80;
    if (used > 56) {
      memset(buf + used, 0, 64 - used);
      block(buf);
      used = 0;
    }
    memset(buf + used, 0, 56 - used);
    for (unsigned i = 0; i < 8; i++)
      buf[63 - i] = uint8_t(bits >> (8 * i));
    block(buf);
    for (unsigned i = 0; i < 32; i++)
      out[i] = uint8_t(h[i / 4] >> (24 - 8 * (i % 4)));
    wipe(this, sizeof(*this));
  }
};
} // namespace
void sha256(const uint8_t *p, size_t n, uint8_t out[32]) {
  Sha s;
  s.add(p, n);
  s.end(out);
}
void hmac(const uint8_t key[32], const uint8_t *p, size_t n, uint8_t out[32]) {
  uint8_t pad[64], inner[32];
  for (unsigned i = 0; i < 64; i++)
    pad[i] = (i < 32 ? key[i] : 0) ^ 0x36;
  Sha a;
  a.add(pad, 64);
  a.add(p, n);
  a.end(inner);
  for (unsigned i = 0; i < 64; i++)
    pad[i] = (i < 32 ? key[i] : 0) ^ 0x5c;
  Sha b;
  b.add(pad, 64);
  b.add(inner, 32);
  b.end(out);
  wipe(pad, 64);
  wipe(inner, 32);
}
} // namespace gw
