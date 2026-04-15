#pragma once
#include <stdint.h>

namespace zoneair {

// Operating mode enum.
// Wire encoding (DP 4 / id 0x0012, type enum):
//   0=Cool, 1=Heat, 2=Fan, 3=Dry, 4=Auto
// Source: /Users/Ben/tuya_complete_reference.md PART 7 lines 376-382 ("send mode,have mode N saved").
// Off is a local-only state; on the wire it is power=0.
enum class Mode : uint8_t { Off = 0, Cool = 1, Heat = 2, Auto = 3, Dry = 4, Fan = 5 };

// Fan speed enum (DP 5 / id 0x0005, type enum).
// Tuya enum index ordering for AC fan is community-conventional:
//   0=Auto, 1=Low, 2=Medium, 3=High
// Pioneer/Daizuki YAML profiles (make-all/tuya-local) use these values.
// We expose F1..F3 + Auto here; quiet/turbo are sent as separate bool DPs.
enum class FanSpeed : uint8_t { Auto = 0, F1 = 1, F2 = 2, F3 = 3, F4 = 4, F5 = 5 };

// vswing_pos (DP 31 / id 0x0011): 0 = off / no swing, 1..5 = fixed positions
// (top..bottom), 6..8 = moving sweeps. We keep the same indices our HTTP API
// already exposes; we map to a single Tuya enum value at send time.

struct AcState {
  bool power;            // DP 1
  Mode mode;             // DP 4
  FanSpeed fan;          // DP 5
  int  setpoint_f;       // DP 24 (Fahrenheit, integer 61..88)
  float indoor_temp_c;   // DP 3 (Celsius)
  bool eco;              // DP 7
  bool turbo;            // DP 8 (Tuya bool — "boost"/turbo)
  bool mute;             // DP 9 (mute / quiet)
  uint8_t vswing_pos;    // DP 31 mapped index, see above
  bool valid;
};

}  // namespace zoneair
