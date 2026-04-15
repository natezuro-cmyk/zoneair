#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../state/ac_state.h"

namespace zoneair {

class TclProtocol {
 public:
  static size_t buildQuery(uint8_t* out, size_t out_cap);
  static size_t buildSet(const AcState& desired, uint8_t* out, size_t out_cap);
  static bool parseState(const uint8_t* in, size_t in_len, AcState& state);
};

}  // namespace zoneair
