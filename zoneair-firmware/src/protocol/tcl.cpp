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

size_t TclProtocol::buildSet(const AcState& desired, uint8_t* out, size_t out_cap) {
  // Base template — 35 bytes.
  // Source: tcl_climate.h set_cmd_base[] line 195.
  static const uint8_t TEMPLATE[35] = {
    0xBB, 0x00, 0x01, 0x03, 0x1D, 0x00, 0x00,  // [0..6] header + type(0x03) + len(0x1D=29)
    0x64,  // [7]  eco=0, disp=1, beep=1, power=1 (will be patched for desired.power)
    0x03,  // [8]  mute=0, turbo=0, mode=0x03 (placeholder, patched below)
    0xF3,  // [9]  upper_nibble=0xF, temp=3 (placeholder, patched below)
    0x00,  // [10] fan=0, vswing=0 (patched below)
    0x00, 0x00, 0x00, 0x00,  // [11..14]
    0x00, 0x00, 0x00, 0x00, 0x00,  // [15..19]
    0x00, 0x00, 0x00, 0x00, 0x00,  // [20..24]
    0x00, 0x00, 0x00, 0x00, 0x00,  // [25..29]
    0x00, 0x00, 0x00, 0x00, 0x00   // [30..34]
  };
  constexpr size_t FRAME_LEN = sizeof(TEMPLATE);
  if (out_cap < FRAME_LEN) return 0;

  std::memcpy(out, TEMPLATE, FRAME_LEN);

  // ---- byte[7]: power bit ----
  // bit2 = power.  Base has 0x64 (power=1); clear or set to match desired.
  // Source: tcl_climate.h set_cmd_t data.power (bit2 of byte7), line 129.
  //         tcl_climate.cpp build_set_cmd() line 99.
  if (desired.power) {
    out[7] |= 0x04;   // set bit2
  } else {
    out[7] &= ~0x04;  // clear bit2
  }

  // ---- byte[8]: mode ----
  // bits[3:0] = mapped mode code.
  // Source: tcl_climate.cpp build_set_cmd() lines 109-120 MODE_MAP.
  // Mapping (get_resp mode → set_cmd mode):
  //   Cool(get_resp=0x01) → 0x03 | Fan(0x02) → 0x02 | Dry(0x03) → 0x07
  //   Heat(get_resp=0x04) → 0x01 | Auto(get_resp=0x05) → 0x08
  // Our AcState modes map to get_resp codes: Cool→0x01, Fan→0x02, Dry→0x03,
  //   Heat→0x04, Auto→0x05.
  {
    uint8_t mode_code;
    switch (desired.mode) {
      case Mode::Cool: mode_code = 0x03; break;  // MODE_MAP[1]
      case Mode::Fan:  mode_code = 0x02; break;  // MODE_MAP[2]
      case Mode::Dry:  mode_code = 0x07; break;  // MODE_MAP[3]
      case Mode::Heat: mode_code = 0x01; break;  // MODE_MAP[4]
      case Mode::Auto: mode_code = 0x08; break;  // MODE_MAP[5]
      default:         mode_code = 0x08; break;  // fallback Auto
    }
    out[8] = (out[8] & 0xF0) | (mode_code & 0x0F);
  }

  // ---- byte[9]: temp field ----
  // bits[3:0] = 15 - (setpoint - 16) = 31 - setpoint  (integer part only).
  // Source: tcl_climate.cpp build_set_cmd() line 123:
  //   set_cmd.temp = 15 - get_cmd_resp->data.temp
  //   where get_cmd_resp->data.temp = setpoint - 16.
  {
    int setpoint_whole = static_cast<int>(desired.setpoint_c);  // truncate toward 0
    int temp_field = setpoint_whole - 16;                        // get_resp encoding
    int set_temp   = 15 - temp_field;                            // set_cmd encoding
    out[9] = (out[9] & 0xF0) | (static_cast<uint8_t>(set_temp) & 0x0F);
  }

  // ---- byte[14]: half_degree bit ----
  // bit5 = 1 if setpoint has a .5°C fractional part.
  // Source: tcl_climate.h set_cmd_t data.half_degree bit5 of byte14, line 158.
  //         tcl_climate.cpp build_set_cmd() line 160 (cleared by default;
  //         caller sets it before calling build_set_cmd per ESPHome control()).
  {
    float frac = desired.setpoint_c - static_cast<int>(desired.setpoint_c);
    if (frac >= 0.4f) {
      out[14] |= 0x20;   // set bit5
    } else {
      out[14] &= ~0x20;  // clear bit5
    }
  }

  // ---- byte[10]: fan ----
  // bits[2:0] = mapped fan code.
  // Source: tcl_climate.cpp build_set_cmd() lines 126-137 FAN_MAP.
  // get_resp fan codes → set_cmd fan codes:
  //   Auto(0x00)→0x00 | Low(0x01)→0x02 | Med(0x02)→0x03
  //   High(0x03)→0x05 | Low2(0x04)→0x06 | High2(0x05)→0x07
  // For our enum: Auto→0x00, Low→0x02 (FAN_MAP[1]), Med→0x03 (FAN_MAP[2]),
  //   High→0x05 (FAN_MAP[3]) — fixture byte[10]=0x05 confirms High→0x05.
  {
    uint8_t fan_code;
    switch (desired.fan) {
      case FanSpeed::Auto: fan_code = 0x00; break;  // FAN_MAP[0]
      case FanSpeed::Low:  fan_code = 0x02; break;  // FAN_MAP[1]
      case FanSpeed::Med:  fan_code = 0x03; break;  // FAN_MAP[2]
      case FanSpeed::High: fan_code = 0x05; break;  // FAN_MAP[3] — fixture confirmed
      default:             fan_code = 0x00; break;
    }
    out[10] = (out[10] & 0xF8) | (fan_code & 0x07);
  }

  // ---- byte[34]: XOR checksum ----
  // XOR of bytes [0..33], stored at byte [34].
  // Source: tcl_climate.cpp build_set_cmd() lines 163-167.
  out[FRAME_LEN - 1] = tcl_xor_checksum(out, FRAME_LEN - 1);

  return FRAME_LEN;
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
