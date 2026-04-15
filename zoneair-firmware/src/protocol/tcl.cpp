#include "tcl.h"
#include <cstring>

namespace zoneair {

// XOR of bytes [0..len-1].
// Source: lNikazzzl-tcl_ac_esphome/components/tcl_climate/tcl_climate.cpp
//   build_set_cmd() lines 163-167, is_valid_xor() lines 377-385.
static uint8_t tcl_xor_checksum(const uint8_t* data, size_t len) {
  uint8_t s = 0;
  for (size_t i = 0; i < len; ++i) s ^= data[i];
  return s;
}

size_t TclProtocol::buildQuery(uint8_t* out, size_t out_cap) {
  // Bytes 0..6 of REQ_CMD from tcl_climate.cpp line 12.
  static const uint8_t TEMPLATE[] = { 0xBB, 0x00, 0x01, 0x04, 0x02, 0x01, 0x00 };
  constexpr size_t LEN_NO_CHK = sizeof(TEMPLATE);
  if (out_cap < LEN_NO_CHK + 1) return 0;
  std::memcpy(out, TEMPLATE, LEN_NO_CHK);
  out[LEN_NO_CHK] = tcl_xor_checksum(out, LEN_NO_CHK);
  return LEN_NO_CHK + 1;
}

size_t TclProtocol::buildSet(const AcState&, uint8_t*, size_t) {
  return 0;  // implemented in Task 5
}

bool TclProtocol::parseState(const uint8_t*, size_t, AcState&) {
  return false;  // implemented in Task 4
}

}  // namespace zoneair
