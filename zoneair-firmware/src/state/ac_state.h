#pragma once
#include <stdint.h>

namespace zoneair {

enum class Mode : uint8_t { Off = 0, Cool = 1, Heat = 2, Auto = 3, Dry = 4, Fan = 5 };

// 6-speed fan enum matching reference FAN_MODE_MAP (tcl_climate.cpp:433).
// Wire get-resp codes: Auto=0x00, F1=0x01, F2=0x04, F3=0x02, F4=0x05, F5=0x03.
enum class FanSpeed : uint8_t { Auto = 0, F1 = 1, F2 = 2, F3 = 3, F4 = 4, F5 = 5 };

// vswing_pos: 0=Last (no change), 1=Fix top, 2=Fix upper, 3=Fix mid,
//             4=Fix lower, 5=Fix bottom, 6=Move full, 7=Move upper, 8=Move lower.
// Matches tcl-ac.yml select options and control_vertical_swing() mapping.

struct AcState {
  bool power;
  Mode mode;
  FanSpeed fan;
  float setpoint_c;
  float indoor_temp_c;
  bool eco;         // eco/night bit — get byte7[6], set byte7[7]
  bool turbo;       // turbo burst  — get byte7[7], set byte8[6]
  bool mute;        // quiet mode   — get byte33[7], set byte8[7]
  uint8_t vswing_pos; // 0..8, see comment above
  bool valid;
};

}  // namespace zoneair
