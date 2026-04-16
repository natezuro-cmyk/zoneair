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
  int   setpoint_f;       // if > 0 and use_fahrenheit, send native F via byte[12]=0x80
  bool  use_fahrenheit;
  float indoor_temp_c;
  bool eco;         // eco/night bit — get byte7[6], set byte7[7]
  bool turbo;       // turbo burst  — get byte7[7], set byte8[6]
  bool mute;        // quiet mode   — get byte33[7], set byte8[7]
  uint8_t vswing_pos; // 0..8, see comment above
  bool    display;    // panel LED display on/off — set byte[7] bit 6
  bool    beep;       // beep/buzzer on/off       — set byte[7] bit 5
  bool    off_timer_en;    // byte[7] bit 3 (junkfix comment, untested for LED)
  bool    on_timer_en;     // byte[7] bit 4 (junkfix comment, untested for LED)
  bool    timer_indicator; // byte[10] bit 6 (junkfix's "timerindicator?")
  uint8_t off_timer_hours; // byte 13, range 0-24, per /Users/Ben/zone-air/bb_protocol.h:204
  uint8_t on_timer_hours;  // byte 14, range 0-24, per /Users/Ben/zone-air/bb_protocol.h:205
  bool valid;
};

}  // namespace zoneair
