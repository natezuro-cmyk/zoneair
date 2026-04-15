#pragma once
#include "../state/ac_state.h"

namespace zoneair {
class WsServer {
 public:
  void begin();
  void pushState(const AcState& s);
};
}
