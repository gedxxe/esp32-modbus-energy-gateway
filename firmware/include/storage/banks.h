// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "core/bytes.h"
namespace gw {
constexpr size_t MaxBlob = 4096;
class Backend {
public:
  virtual ~Backend() = default;
  virtual size_t size(const char *key) = 0;
  virtual size_t read(const char *key, uint8_t *p, size_t n) = 0;
  virtual size_t write(const char *key, const uint8_t *p, size_t n) = 0;
};
enum class Load { Empty, Ok, Corrupt };
// Owner-only buffers: one storage worker, never ISR/radio/HTTP concurrent access.
class Banks {
  Backend &backend;
  const char *a;
  const char *b;
  uint8_t kind;
  int bank = -1;
  uint32_t generation = 0;
  // All instances share scratch space: exclusively called by the storage owner.
  static uint8_t bytes[MaxBlob], verify[MaxBlob];
  bool valid(const uint8_t *p, size_t n) const;

public:
  Banks(Backend &io, const char *ka, const char *kb, uint8_t type)
      : backend(io), a(ka), b(kb), kind(type) {}
  Load load(uint8_t *out, size_t n, bool strict = false);
  bool save(const uint8_t *p, size_t n);
};
} // namespace gw
