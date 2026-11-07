// SPDX-License-Identifier: GPL-3.0-only
// Immutable gateway-side IDs; CI verifies them against the read-only client's MeterProfiles.h.
#pragma once
#include <cstdint>
namespace gw { namespace meter_profile {
constexpr uint8_t Fort = 0, Adl400Historical = 1, Habuer = 2, Acr = 3, Apm = 4;
constexpr uint8_t Adl400Verified = 5, SchneiderDelivered = 6;
constexpr uint8_t SchneiderDeliveredReceived = 7, Count = 8;
inline bool valid(uint8_t id) { return id < Count; }
inline bool floatEnergy(uint8_t id) {
  return id == Acr || id == Apm || id == SchneiderDelivered ||
         id == SchneiderDeliveredReceived;
}
inline bool unityRatio(uint8_t id) { return id == Habuer || floatEnergy(id); }
} } // namespace gw::meter_profile
