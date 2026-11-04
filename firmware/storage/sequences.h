// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "banks.h"
namespace gw {
// Never evict entries silently: recorded HELLOs must not recreate a low POLL sequence.
constexpr size_t SessionCapacity = 64, LedgerBytes = SessionCapacity * 16;
constexpr uint32_t SequenceBlock = 64;
struct Reservation {
  uint16_t network = 0, node = 0;
  uint64_t session = 0;
  uint32_t high = 0, next = 0;
};
class Sequences {
  Banks &banks;
  Reservation entries[SessionCapacity]{};
  uint8_t wire[LedgerBytes]{};
  bool persist();

public:
  bool healthy = false;
  size_t used = 0;
  explicit Sequences(Banks &b) : banks(b) {}
  bool load(bool newInstallation);
  bool take(uint16_t network, uint16_t node, uint64_t session, uint32_t &sequence);
};
} // namespace gw
