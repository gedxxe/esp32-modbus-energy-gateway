// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "protocol.h"
#include <cstdint>

namespace gw {
// CSMA/CA-like channel access for discretionary gateway downlinks only.
// ACKs must not wait for CAD because a node is already waiting for them.
// SX1262 CAD is carrier activity detection, NOT hidden-terminal protection
// and NOT a reservation or a time slot.
constexpr uint32_t CadTimeoutMs = 1500;
constexpr uint32_t CadBaseDelayMs = 300;
constexpr uint32_t CadMaxWindowMs = 12000;
constexpr uint8_t CadMaxExponent = 4;

inline bool requiresChannelCheck(uint8_t type) { return type == Poll; }
inline uint32_t channelBackoffMs(uint32_t entropy, uint8_t collisions) {
  uint8_t factor = collisions > CadMaxExponent ? CadMaxExponent : collisions;
  uint32_t window = 800u << factor;
  if (window > CadMaxWindowMs)
    window = CadMaxWindowMs;
  return CadBaseDelayMs + entropy % window;
}
// Keep the chosen entropy injection external: the portable core is testable,
// and the ESP32 radio owner supplies hardware entropy via esp_random().
} // namespace gw
