#pragma once
#include "../state/ac_state.h"
#include <functional>

namespace zoneair {

class HttpServer {
 public:
  using SetCommandHandler = std::function<void(const AcState& desired)>;
  void begin(const AcState* current_state, SetCommandHandler on_set);
};

}
