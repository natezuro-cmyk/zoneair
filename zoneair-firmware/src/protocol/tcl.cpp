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

// Minimum response frame length: 61 bytes.
// Source: tcl_climate.h get_cmd_resp_t raw[61]; tcl_climate.cpp loop() line 405
//   (len == sizeof(m_get_cmd_resp) && buffer[3] == 0x04).
static constexpr size_t TCL_RESP_LEN = 61;

bool TclProtocol::parseState(const uint8_t* in, size_t in_len, AcState& state) {
  state.valid = false;

  // Reject frames shorter than the expected response size.
  if (in_len < TCL_RESP_LEN) return false;

  // Reject if frame type byte is not 0x04.
  // Source: tcl_climate.cpp loop() line 405: buffer[3] == 0x04
  if (in[3] != 0x04) return false;

  // Validate XOR checksum: XOR of bytes [0..len-2] must equal byte [len-1].
  // Source: tcl_climate.cpp is_valid_xor() lines 377-385.
  if (tcl_xor_checksum(in, in_len - 1) != in[in_len - 1]) return false;

  // ---- Byte 7 ----
  // Bits [3:0] = mode, bit [4] = power.
  // Source: tcl_climate.h get_cmd_resp_t data.mode (4-bit), data.power (1-bit).
  uint8_t mode_raw  = in[7] & 0x0F;
  bool    power     = (in[7] >> 4) & 0x01;

  // ---- Byte 8 ----
  // Bits [3:0] = temp field, bits [6:4] = fan.
  // Source: tcl_climate.h get_cmd_resp_t data.temp (4-bit), data.fan (3-bit).
  uint8_t temp_field = in[8] & 0x0F;
  uint8_t fan_raw    = (in[8] >> 4) & 0x07;

  // ---- Setpoint ----
  // target_temperature = temp_field + 16
  // Source: tcl_climate.cpp loop() line 493.
  float setpoint = static_cast<float>(temp_field + 16);

  // ---- Indoor temp ----
  // raw = (buffer[17] << 8) | buffer[18]
  // curr_temp = (raw / 374.0 - 32.0) / 1.8
  // Source: tcl_climate.cpp loop() line 412.
  uint16_t raw_temp = (static_cast<uint16_t>(in[17]) << 8) | in[18];
  float indoor_temp = (raw_temp / 374.0f - 32.0f) / 1.8f;

  // ---- Mode mapping ----
  // Source: tcl_climate.cpp loop() lines 419-429 MODE_MAP.
  // Reference codes: 0x01=Cool, 0x02=FanOnly, 0x03=Dry, 0x04=Heat, 0x05=Auto.
  Mode mode;
  if (!power) {
    mode = Mode::Off;
  } else {
    switch (mode_raw) {
      case 0x01: mode = Mode::Cool; break;
      case 0x02: mode = Mode::Fan;  break;
      case 0x03: mode = Mode::Dry;  break;
      case 0x04: mode = Mode::Heat; break;
      case 0x05: mode = Mode::Auto; break;
      default:   mode = Mode::Off;  break;  // unknown → Off, still accept frame
    }
  }

  // ---- Fan mapping ----
  // Source: tcl_climate.cpp loop() lines 433-440 FAN_MODE_MAP.
  // Reference codes: 0x00=Auto, 0x01="1"(Low), 0x04="2"(Low), 0x02="3"(Med),
  //                  0x05="4"(High), 0x03="5"(High).
  FanSpeed fan;
  switch (fan_raw) {
    case 0x00: fan = FanSpeed::Auto; break;
    case 0x01: fan = FanSpeed::Low;  break;
    case 0x04: fan = FanSpeed::Low;  break;
    case 0x02: fan = FanSpeed::Med;  break;
    case 0x05: fan = FanSpeed::High; break;
    case 0x03: fan = FanSpeed::High; break;
    default:   fan = FanSpeed::Auto; break;  // unknown → Auto, still accept frame
  }

  state.power        = power;
  state.mode         = mode;
  state.fan          = fan;
  state.setpoint_c   = setpoint;
  state.indoor_temp_c = indoor_temp;
  state.valid        = true;
  return true;
}

}  // namespace zoneair
