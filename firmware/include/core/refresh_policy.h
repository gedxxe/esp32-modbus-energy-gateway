// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>

namespace gw {
// LoRa v1 has no remote interval-setting command. A node may transmit once
// per hour on its own; request a live sample only after its uplinks have gone
// missing. The grace window avoids polling at the same time as that uplink.
constexpr uint64_t RefreshIntervalMs = 3600000ULL;
constexpr uint64_t RefreshGraceMs = 120000ULL;
constexpr uint64_t RefreshRetryMs = 300000ULL;
constexpr uint64_t RefreshSpacingMs = 45000ULL;

class RefreshPolicy {
  uint64_t retryAfter[16]{};
  uint64_t nextDispatch = 0;
  uint8_t cursor = 0;

public:
  void reset() {
    for (auto &slot : retryAfter) slot = 0;
    nextDispatch = 0;
    cursor = 0;
  }
  bool available(uint64_t now) const { return now >= nextDispatch; }
  uint8_t at(unsigned offset) const { return uint8_t((cursor + offset) % 16); }
  bool due(uint8_t index, uint64_t now, uint64_t received, bool validEnergy) const {
    if (index >= 16 || !received || now < received || now < retryAfter[index] ||
        !available(now))
      return false;
    const uint64_t threshold =
        validEnergy ? RefreshIntervalMs + RefreshGraceMs : RefreshRetryMs;
    return now - received >= threshold;
  }
  void attempted(uint8_t index, uint64_t now) {
    retryAfter[index] = now + RefreshRetryMs;
    nextDispatch = now + RefreshSpacingMs;
    cursor = uint8_t((index + 1) % 16);
  }
};
} // namespace gw
