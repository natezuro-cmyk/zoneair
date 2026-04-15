#pragma once
#include <stdint.h>
#include <stddef.h>

namespace zoneair { namespace fixtures {

// All fixtures derived from:
//   references/lNikazzzl-tcl_ac_esphome/components/tcl_climate/tcl_climate.cpp
//   references/lNikazzzl-tcl_ac_esphome/components/tcl_climate/tcl_climate.h
//
// Frame checksum: XOR of all bytes except the last (tcl_climate.cpp build_set_cmd(),
// is_valid_xor()).  All checksums below are verified.

// ---------------------------------------------------------------------------
// QUERY — periodic poll sent by WiFi module to AC mainboard every 450 ms.
// Source: tcl_climate.cpp line 12
//   static constexpr uint8_t REQ_CMD[] = {0xBB, 0x00, 0x01, 0x04, 0x02, 0x01, 0x00, 0xBD};
// XOR(BB^00^01^04^02^01^00) = BD  ✓
// ---------------------------------------------------------------------------
constexpr uint8_t QUERY[] = {
    0xBB, 0x00, 0x01, 0x04, 0x02, 0x01, 0x00, 0xBD
};
constexpr size_t QUERY_LEN = sizeof(QUERY);

// ---------------------------------------------------------------------------
// RESPONSE_COOL_22_AUTO — state response from AC mainboard.
// Source: tcl_climate.h get_cmd_resp_t (raw[61]) + tcl_climate.cpp loop() parse logic.
//
// Frame anatomy (61 bytes):
//   [0]    = 0xBB  header (tcl_climate.cpp read_data_line: starts frame on 0xBB)
//   [1..2] = 0x00, 0x01
//   [3]    = 0x04  type  (tcl_climate.cpp loop(): buffer[3] == 0x04 guard)
//   [4]    = 0x37  len=55 (61 = 1-byte-header + 3 + 1-len + 55-data + 1-checksum)
//   [5..6] = 0x00, 0x00
//   [7]    = 0x11  turbo[7]=0 | eco[6]=0 | disp[5]=0 | power[4]=1 | mode[3:0]=0x01(Cool)
//                  (tcl_climate.cpp loop() MODE_MAP: {0x01 → CLIMATE_MODE_COOL})
//   [8]    = 0x06  byte_8_bit_7[7]=0 | fan[6:4]=0x00(Auto) | temp[3:0]=0x06
//                  (tcl_climate.cpp loop() line 493: target_temperature = temp + 16;
//                   22°C → temp_field = 22-16 = 6)
//   [9]    = 0x00
//   [10]   = 0x00  vswing=0, hswing=0
//   [11..16] = 0x00
//   [17]   = 0x63  } indoor temp raw MSB  (tcl_climate.cpp loop() line 412:
//   [18]   = 0x58  } indoor temp raw LSB   curr_temp = (raw/374.0 - 32.0) / 1.8
//                  20°C → raw = (20*1.8+32)*374 = 68*374 = 25432 = 0x6358)
//   [19..59] = 0x00
//   [60]   = 0xA5  XOR checksum (verified)
// ---------------------------------------------------------------------------
constexpr uint8_t RESPONSE_COOL_22_AUTO[] = {
    0xBB, 0x00, 0x01, 0x04, 0x37, 0x00, 0x00,
    0x11, // byte[7]: power=1, mode=0x01(Cool)
    0x06, // byte[8]: fan=Auto(0), temp_field=6 (22°C)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x63, 0x58, // bytes[17:18]: indoor temp raw = 25432 → 20.0°C
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // [19..28]
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // [29..38]
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // [39..48]
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // [49..58]
    0x00,                                                         // [59]
    0xA5  // XOR checksum (verified)
};
constexpr size_t RESPONSE_COOL_22_AUTO_LEN = sizeof(RESPONSE_COOL_22_AUTO);

// ---------------------------------------------------------------------------
// SET_HEAT_25_5_HIGH — command sent to AC mainboard: heat mode, 25.5°C, fan speed F5.
// Source: tcl_climate.h set_cmd_t (raw[35]) + set_cmd_base[] (line 195) +
//         tcl_climate.cpp build_set_cmd().
//
// Fan note: FanSpeed::F5 → get_resp wire code 0x03 → FAN_MAP[0x03] = 0x05 (set-cmd code).
// This matches the old "High" fan mapping; the byte value 0x05 at [10] is unchanged.
//
// Derivation:
//   Base: set_cmd_base[] = {0xBB,0x00,0x01,0x03,0x1D,0x00,0x00,0x64,0x03,0xF3,...zeros}
//
//   [0..4]  = 0xBB,0x00,0x01,0x03,0x1D  header+type(0x03)+len(29=0x1D)
//   [5..6]  = 0x00,0x00
//   [7]     = 0x64  eco[7]=0|disp[6]=1|beep[5]=1|on_timer[4]=0|off_timer[3]=0|power[2]=1|bits[1:0]=0
//                   (build_set_cmd(): beep=1, disp=1, power=1; base confirms 0x64)
//   [8]     = 0x01  mute[7]=0|turbo[6]=0|bits[5:4]=0|mode[3:0]=0x01
//                   (build_set_cmd() MODE_MAP: get_resp.mode=0x04(Heat)→set_cmd.mode=MODE_MAP[4]=0x01)
//   [9]     = 0xF6  byte_9_bit_4_7[7:4]=0xF(from base 0xF3)|temp[3:0]=0x06
//                   (build_set_cmd(): set_cmd.temp = 15 - get_resp.temp
//                    get_resp.temp = 25-16=9; set_cmd.temp = 15-9 = 6 = 0x06)
//   [10]    = 0x05  fan[2:0]=0x05|vswing[5:3]=0
//                   (build_set_cmd() FAN_MAP: FanSpeed::F5→get_resp.fan=0x03→FAN_MAP[3]=0x05)
//   [11]    = 0x00  hswing=0
//   [12..13]= 0x00, 0x00
//   [14]    = 0x20  half_degree[5]=1 (for the .5 in 25.5°C), all other bits 0
//                   (set_cmd_t.data.half_degree, bit 5 of byte 14)
//   [15..31]= 0x00  (from set_cmd_base zeros)
//   [32]    = 0x00  vswing_fix=0, vswing_mv=0
//   [33]    = 0x00  hswing_fix=0, hswing_mv=0
//   [34]    = 0x12  XOR checksum (verified)
// ---------------------------------------------------------------------------
constexpr uint8_t SET_HEAT_25_5_HIGH[] = {
    0xBB, 0x00, 0x01, 0x03, 0x1D, 0x00, 0x00,
    0x64, // byte[7]:  eco=0, disp=1, beep=1, power=1
    0x01, // byte[8]:  mute=0, turbo=0, mode=0x01(Heat after MODE_MAP)
    0xF6, // byte[9]:  upper_nibble=0xF(base), temp_field=6 (25°C integer)
    0x05, // byte[10]: fan=0x05(FAN_MAP["5"/High]), vswing=0
    0x00, // byte[11]: hswing=0
    0x00, 0x00,
    0x20, // byte[14]: half_degree=1 (bit5) → 0.5°C extra = 25.5°C total
    0x00, 0x00, 0x00, 0x00, 0x00, // [15..19]
    0x00, 0x00, 0x00, 0x00, 0x00, // [20..24]
    0x00, 0x00, 0x00, 0x00, 0x00, // [25..29]
    0x00, 0x00,                    // [30..31]
    0x00, // byte[32]: vswing_fix=0, vswing_mv=0
    0x00, // byte[33]: hswing_fix=0, hswing_mv=0
    0x12  // XOR checksum (verified)
};
constexpr size_t SET_HEAT_25_5_HIGH_LEN = sizeof(SET_HEAT_25_5_HIGH);

}}  // namespace zoneair::fixtures
