// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "banks.h"
#include "core/checkpoint.h"
namespace gw {
constexpr size_t ArchiveCapacity = 64, ArchiveBytes = 494;
struct ArchiveInfo {
  bool used = false;
  uint16_t network = 0, node = 0;
  uint32_t day = 0, revision = 0;
  uint64_t boot = 0, received = 0, session = 0;
  uint32_t correlation = 0;
};
class Archives {
  Backend &backend;
  uint8_t bytes[ArchiveBytes]{};
  void keys(unsigned i, char a[12], char b[12]) const;

public:
  ArchiveInfo entries[ArchiveCapacity]{};
  size_t count = 0;
  uint32_t errors = 0;
  bool healthy = false;
  explicit Archives(Backend &io) : backend(io) {}
  bool load();
  bool save(uint16_t network, uint16_t node, uint64_t session, uint32_t correlation,
            const uint8_t record[446], uint64_t boot, uint64_t received);
  bool read(unsigned index, uint8_t out[ArchiveBytes]);
};
} // namespace gw
