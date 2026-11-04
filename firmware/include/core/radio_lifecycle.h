// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>
namespace gw {
enum class RadioPhase { Disabled, ResetLow, ResetWait, BusyWait, Initializing, Rx, Cad, Tx, Recovery };
enum class RadioAction { None, ReleaseReset, Initialize, BusyTimedOut, Restart };
// Driver-independent clock used by the actual radio owner and absent-module regression.
class RadioLifecycle {
public:
  RadioPhase phase = RadioPhase::Disabled;
  uint64_t due = 0;
  void start(bool enabled, uint64_t now) {
    phase = enabled ? RadioPhase::ResetLow : RadioPhase::Disabled;
    due = now + 2;
  }
  void fail(uint64_t now) {
    phase = RadioPhase::Recovery;
    due = now + 30000;
  }
  RadioAction step(uint64_t now, bool busy) {
    if (phase == RadioPhase::ResetLow && now >= due) {
      phase = RadioPhase::ResetWait;
      due = now + 10;
      return RadioAction::ReleaseReset;
    }
    if (phase == RadioPhase::ResetWait && now >= due) {
      phase = RadioPhase::BusyWait;
      due = now + 100;
    }
    if (phase == RadioPhase::BusyWait) {
      if (!busy) {
        phase = RadioPhase::Initializing;
        return RadioAction::Initialize;
      }
      if (now >= due)
        return RadioAction::BusyTimedOut;
    }
    if (phase == RadioPhase::Recovery && now >= due)
      return RadioAction::Restart;
    return RadioAction::None;
  }
  static bool txExpired(uint64_t now, uint64_t started, uint32_t airMs) {
    return now - started > uint64_t(airMs) + 2000;
  }
};
} // namespace gw
