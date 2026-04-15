#pragma once
#include <stdint.h>

namespace zoneair {

enum class Mode : uint8_t { Off = 0, Cool = 1, Heat = 2, Auto = 3, Dry = 4, Fan = 5 };
enum class FanSpeed : uint8_t { Auto = 0, Low = 1, Med = 2, High = 3 };

struct AcState {
  bool power;
  Mode mode;
  FanSpeed fan;
  float setpoint_c;
  float indoor_temp_c;
  bool valid;
};

}  // namespace zoneair
