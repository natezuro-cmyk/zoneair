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

  // ---- byte[7]: eco bit ----
  // bit7 = eco.  Source: tcl_climate.h set_cmd_t data.eco (bit7 of byte7), line 133.
  //              tcl_climate.cpp build_set_cmd() line 104 (eco hardcoded 0 in reference;
  //              we honour desired.eco).
  if (desired.eco) {
    out[7] |= 0x80;
  } else {
    out[7] &= ~0x80;
  }

  // ---- byte[8]: turbo + mute bits ----
  // bit6 = turbo, bit7 = mute.
  // Source: tcl_climate.h set_cmd_t data.turbo (bit6 byte8) line 137,
  //         data.mute (bit7 byte8) line 138.
  //         tcl_climate.cpp build_set_cmd() lines 105-106.
  if (desired.turbo) {
    out[8] |= 0x40;
  } else {
    out[8] &= ~0x40;
  }
  if (desired.mute) {
    out[8] |= 0x80;
  } else {
    out[8] &= ~0x80;
  }

  // ---- byte[10]: fan (bits[2:0]) and vswing enable (bits[5:3]) ----
  // Fan mapping via FAN_MAP (tcl_climate.cpp build_set_cmd() lines 126-137).
  // AcState fan enum → get_resp wire code → set_cmd wire code:
  //   Auto(0)→0x00→0x00 | F1(1)→0x01→0x02 | F2(2)→0x04→0x06
  //   F3(3)→0x02→0x03   | F4(4)→0x05→0x07 | F5(5)→0x03→0x05
  // Reference FAN_MAP indexed by get_resp code:
  //   [0x00]=0x00, [0x01]=0x02, [0x02]=0x03, [0x03]=0x05, [0x04]=0x06, [0x05]=0x07
  {
    uint8_t fan_code;
    switch (desired.fan) {
      case FanSpeed::Auto: fan_code = 0x00; break;  // FAN_MAP[0x00]
      case FanSpeed::F1:   fan_code = 0x02; break;  // FAN_MAP[0x01]
      case FanSpeed::F2:   fan_code = 0x06; break;  // FAN_MAP[0x04]
      case FanSpeed::F3:   fan_code = 0x03; break;  // FAN_MAP[0x02]
      case FanSpeed::F4:   fan_code = 0x07; break;  // FAN_MAP[0x05]
      case FanSpeed::F5:   fan_code = 0x05; break;  // FAN_MAP[0x03] — fixture confirmed
      default:             fan_code = 0x00; break;
    }
    out[10] = (out[10] & 0xF8) | (fan_code & 0x07);
  }

  // ---- vswing enable (byte[10] bits[5:3]) and vswing_fix/vswing_mv (byte[32]) ----
  // Source: tcl_climate.h set_cmd_t data.vswing (bits[5:3] byte10), line 144.
  //         data.vswing_fix (bits[2:0] byte32) line 180, data.vswing_mv (bits[4:3] byte32) line 181.
  //         tcl_climate.cpp build_set_cmd() lines 140-148, control_vertical_swing() lines 175-196.
  // vswing_pos encoding: 0=Last, 1=Fix top, 2=Fix upper, 3=Fix mid,
  //   4=Fix lower, 5=Fix bottom, 6=Move full, 7=Move upper, 8=Move lower.
  {
    uint8_t vswing_en  = 0;
    uint8_t vswing_fix = 0;
    uint8_t vswing_mv  = 0;
    switch (desired.vswing_pos) {
      case 1: vswing_fix = 0x01; break;  // Fix top
      case 2: vswing_fix = 0x02; break;  // Fix upper
      case 3: vswing_fix = 0x03; break;  // Fix mid
      case 4: vswing_fix = 0x04; break;  // Fix lower
      case 5: vswing_fix = 0x05; break;  // Fix bottom
      case 6: vswing_mv  = 0x01; vswing_en = 0x07; break;  // Move full
      case 7: vswing_mv  = 0x02; vswing_en = 0x07; break;  // Move upper
      case 8: vswing_mv  = 0x03; vswing_en = 0x07; break;  // Move lower
      default: break;  // 0 = Last — leave fix/mv/en at 0
    }
    // byte[10] bits[5:3] = vswing enable field (0x07 when moving, 0 when fixed/off)
    out[10] = (out[10] & 0xC7) | ((vswing_en & 0x07) << 3);
    // byte[32] bits[2:0] = vswing_fix, bits[4:3] = vswing_mv
    out[32] = (out[32] & 0xE0) | (vswing_fix & 0x07) | ((vswing_mv & 0x03) << 3);
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
  // Bits [3:0] = mode, bit [4] = power, bit [6] = eco, bit [7] = turbo.
  // Source: tcl_climate.h get_cmd_resp_t data.mode (4-bit) line 31,
  //         data.power (bit4) line 32, data.eco (bit6) line 34, data.turbo (bit7) line 35.
  uint8_t mode_raw  = in[7] & 0x0F;
  bool    power     = (in[7] >> 4) & 0x01;
  bool    eco       = (in[7] >> 6) & 0x01;
  bool    turbo     = (in[7] >> 7) & 0x01;

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

  // ---- Byte 33 ----
  // bit [7] = mute.
  // Source: tcl_climate.h get_cmd_resp_t data.mute (bit7 byte33) line 74.
  //         tcl_climate.cpp loop() line 447 (mute check before fan mode decode).
  bool mute = (in[33] >> 7) & 0x01;

  // ---- Byte 51 ----
  // bits [2:0] = vswing_fix, bits [4:3] = vswing_mv.
  // Source: tcl_climate.h get_cmd_resp_t data.vswing_fix (bits[2:0] byte51) line 96,
  //         data.vswing_mv (bits[4:3] byte51) line 97.
  //         tcl_climate.cpp loop() lines 472-480 (set_vswing_pos decode).
  uint8_t vswing_fix = in[51] & 0x07;
  uint8_t vswing_mv  = (in[51] >> 3) & 0x03;

  uint8_t vswing_pos;
  if      (vswing_mv  == 0x01) vswing_pos = 6;  // Move full
  else if (vswing_mv  == 0x02) vswing_pos = 7;  // Move upper
  else if (vswing_mv  == 0x03) vswing_pos = 8;  // Move lower
  else if (vswing_fix == 0x01) vswing_pos = 1;  // Fix top
  else if (vswing_fix == 0x02) vswing_pos = 2;  // Fix upper
  else if (vswing_fix == 0x03) vswing_pos = 3;  // Fix mid
  else if (vswing_fix == 0x04) vswing_pos = 4;  // Fix lower
  else if (vswing_fix == 0x05) vswing_pos = 5;  // Fix bottom
  else                          vswing_pos = 0;  // Last (no swing position set)

  // ---- Fan mapping ----
  // Source: tcl_climate.cpp loop() lines 433-440 FAN_MODE_MAP.
  // Reference wire codes: 0x00=Auto("Automatic"), 0x01=F1("1"), 0x04=F2("2"),
  //                       0x02=F3("3"), 0x05=F4("4"), 0x03=F5("5").
  // turbo/mute take priority over fan code per loop() lines 444-458.
  FanSpeed fan;
  switch (fan_raw) {
    case 0x00: fan = FanSpeed::Auto; break;
    case 0x01: fan = FanSpeed::F1;   break;
    case 0x04: fan = FanSpeed::F2;   break;
    case 0x02: fan = FanSpeed::F3;   break;
    case 0x05: fan = FanSpeed::F4;   break;
    case 0x03: fan = FanSpeed::F5;   break;
    default:   fan = FanSpeed::Auto; break;  // unknown → Auto, still accept frame
  }

  state.power         = power;
  state.mode          = mode;
  state.fan           = fan;
  state.setpoint_c    = setpoint;
  state.indoor_temp_c = indoor_temp;
  state.eco           = eco;
  state.turbo         = turbo;
  state.mute          = mute;
  state.vswing_pos    = vswing_pos;
  state.valid         = true;
  return true;
}

}  // namespace zoneair
