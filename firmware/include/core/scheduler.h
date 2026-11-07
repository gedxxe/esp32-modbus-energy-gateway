// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "protocol.h"
#include "config.h"
namespace gw {
constexpr size_t TxCapacity = 32;
// Keep one reserved ACK queue slot per configured node identity. The other
// half of the 32-job queue can hold at most one POLL per inventory slot.
constexpr size_t TxAckReserve = InventoryCapacity;
uint32_t airtimeUs(const Phy &p, size_t bytes);
struct TxJob {
  bool used = false;
  uint8_t node = 0;
  Frame frame{};
  uint64_t expires = 0, ready = 0;
  uint32_t revision = 0;
  uint8_t cadDeferrals = 0;
};
class Scheduler {
  TxJob jobs[TxCapacity]{};
  uint8_t cursor = 0;

public:
  uint64_t nextTx = 0;
  uint32_t drops = 0, expired = 0, coalesced = 0;
  size_t depth = 0;
  bool enqueue(const TxJob &job);
  // Protect queued ACK/POLL frames from expiry during an outstanding ACK burst.
  uint64_t protectDutyQueue(uint64_t now, const Phy &phy);
  bool pop(uint64_t now, TxJob &out);
  void charge(uint64_t now, uint32_t airtimeMs) {
    nextTx = now + uint64_t(airtimeMs) * 100;
  }
  void cancelNode(uint8_t index);
  void clear();
};
} // namespace gw
