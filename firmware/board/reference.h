#pragma once
// Reference: ESP32-C3-DevKitM-1 / 4 MB / no PSRAM. GPIO labels, not header ordinal.
// Ra-01SH V1.1 crystal + DIO2 internal RF switch. Verify purchased circuit first.
namespace gw {
namespace board {
constexpr int sck = 4, miso = 5, mosi = 6, cs = 7, reset = 10, busy = 3, dio1 = 1;
constexpr bool dio2RfSwitch = true;
constexpr float tcxoVoltage = 0.0f;
constexpr const char *name = "ESP32-C3-DevKitM-1 + Ra-01SH V1.1";
} // namespace board
} // namespace gw
