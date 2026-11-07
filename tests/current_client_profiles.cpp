// SPDX-License-Identifier: GPL-3.0-only
// Compile gateway IDs directly against current client production MeterProfiles.h.
#include "core/meter_profiles.h"
#include "MeterProfiles.h"
#include <iostream>
#include <cmath>
#include <cstring>
static_assert(fort::profiles::FORT == gw::meter_profile::Fort, "FORT identity drift");
static_assert(fort::profiles::ACREL_ADL400 == gw::meter_profile::Adl400Historical, "ADL legacy drift");
static_assert(fort::profiles::HABUER == gw::meter_profile::Habuer, "Habuer drift");
static_assert(fort::profiles::ACREL_ACR == gw::meter_profile::Acr, "ACR drift");
static_assert(fort::profiles::ACREL_APM == gw::meter_profile::Apm, "APM drift");
static_assert(fort::profiles::ACREL_ADL400_VERIFIED == gw::meter_profile::Adl400Verified, "ADL verified drift");
static_assert(fort::profiles::SCHNEIDER_PM5100 == gw::meter_profile::SchneiderDelivered, "PM5100 D drift");
static_assert(fort::profiles::SCHNEIDER_PM5100_DR == gw::meter_profile::SchneiderDeliveredReceived, "PM5110 D+R drift");
static_assert(fort::profiles::COUNT == gw::meter_profile::Count, "Profile COUNT drift");
int main() {
  for (uint8_t id = 0; id < gw::meter_profile::Count; ++id) {
    const auto &p = fort::profiles::get(id);
    if (p.id != id || !gw::meter_profile::valid(id)) return 1;
    if (id == 6 || id == 7) {
      if (p.energy.encoding != fort::profiles::F32 || p.energy.scale != 1.0 ||
          p.ratioLayout != fort::profiles::NO_RATIO) return 2;
      const float reading = 7838.875f;
      uint32_t raw;
      std::memcpy(&raw, &reading, 4);
      const double engineering = fort::profiles::primaryEnergy(id, raw, 1, 1, 1);
      if (std::fabs(engineering - double(reading)) > 1e-9) return 3;
    }
  }
  std::cout << "PASS: all 8 current ESP32-S3 meter profiles match gateway IDs" << std::endl;
}
