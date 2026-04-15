#include <cstdio>
#include <cstring>
#include "../src/protocol/tcl.h"
#include "fixtures/tcl_frames.h"

using namespace zoneair;

#define EXPECT(cond) do { if (!(cond)) { \
  std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
  return 1; } } while(0)

static int test_buildQuery_matches_fixture() {
  uint8_t buf[64];
  size_t n = TclProtocol::buildQuery(buf, sizeof(buf));
  EXPECT(n == fixtures::QUERY_LEN);
  EXPECT(std::memcmp(buf, fixtures::QUERY, n) == 0);
  return 0;
}

// Fixture bytes[17:18] = 0x63, 0x58 → raw = 25432
// curr_temp = (25432 / 374.0 - 32.0) / 1.8 = (68.0 - 32.0) / 1.8 = 20.0°C
// Reference: tcl_climate.cpp loop() line 412.
static int test_parseState_cool_22_auto() {
  AcState s{};
  bool ok = TclProtocol::parseState(
    fixtures::RESPONSE_COOL_22_AUTO,
    fixtures::RESPONSE_COOL_22_AUTO_LEN, s);
  EXPECT(ok);
  EXPECT(s.valid);
  EXPECT(s.power == true);
  EXPECT(s.mode == Mode::Cool);
  EXPECT(s.fan == FanSpeed::Auto);
  EXPECT(s.setpoint_c == 22.0f);
  EXPECT(s.indoor_temp_c == 20.0f);
  return 0;
}

static int test_parseState_rejects_bad_checksum() {
  uint8_t bad[fixtures::RESPONSE_COOL_22_AUTO_LEN];
  std::memcpy(bad, fixtures::RESPONSE_COOL_22_AUTO, sizeof(bad));
  bad[sizeof(bad) - 1] ^= 0xFF;
  AcState s{};
  EXPECT(!TclProtocol::parseState(bad, sizeof(bad), s));
  EXPECT(!s.valid);
  return 0;
}

static int test_buildSet_heat_25_5_high() {
  AcState desired{};
  desired.power = true;
  desired.mode = Mode::Heat;
  desired.fan = FanSpeed::F5;  // F5 → get_resp code 0x03 → FAN_MAP[3] = 0x05 (same wire byte)
  desired.setpoint_c = 25.5f;
  uint8_t buf[64];
  size_t n = TclProtocol::buildSet(desired, buf, sizeof(buf));
  EXPECT(n == fixtures::SET_HEAT_25_5_HIGH_LEN);
  EXPECT(std::memcmp(buf, fixtures::SET_HEAT_25_5_HIGH, n) == 0);
  return 0;
}

// Round-trip test: build a set command with eco/turbo/mute, then parse the
// same bytes as if they were an AC response and confirm the fields match.
// We borrow the 35-byte set frame structure but feed it into parseState via a
// synthetic 61-byte response built from known byte positions.
static int test_eco_turbo_mute_vswing_roundtrip() {
  // Build a synthetic 61-byte response with:
  //   eco=1  (byte7 bit6  = 0x40)
  //   turbo=1 (byte7 bit7 = 0x80)
  //   power=1 (byte7 bit4 = 0x10), mode=Cool (0x01)
  //   mute=1  (byte33 bit7 = 0x80)
  //   vswing_fix=0x03 → vswing_pos=3 (Fix mid) in byte51 bits[2:0]
  //   fan=0x01 → F1 (byte8 bits[6:4]=0x01)
  //   temp=6 (22°C), indoor raw 0x6358 (20°C)
  uint8_t frame[61];
  std::memset(frame, 0x00, sizeof(frame));
  frame[0] = 0xBB;
  frame[1] = 0x00;
  frame[2] = 0x01;
  frame[3] = 0x04;  // type = response
  frame[4] = 0x37;  // len=55
  // byte[7]: turbo=1(bit7), eco=1(bit6), power=1(bit4), mode=Cool(0x01)
  frame[7] = 0x80 | 0x40 | 0x10 | 0x01;  // 0xD1
  // byte[8]: fan=0x01 (bits[6:4]), temp_field=6 (22°C)
  frame[8] = (0x01 << 4) | 0x06;          // 0x16
  // byte[17:18]: indoor temp raw = 25432 = 0x6358 → 20.0°C
  frame[17] = 0x63;
  frame[18] = 0x58;
  // byte[33]: mute=1 (bit7)
  frame[33] = 0x80;
  // byte[51]: vswing_fix=0x03 (bits[2:0])
  frame[51] = 0x03;
  // XOR checksum at byte[60]
  uint8_t chk = 0;
  for (size_t i = 0; i < 60; ++i) chk ^= frame[i];
  frame[60] = chk;

  AcState s{};
  bool ok = TclProtocol::parseState(frame, sizeof(frame), s);
  EXPECT(ok);
  EXPECT(s.valid);
  EXPECT(s.eco   == true);
  EXPECT(s.turbo == true);
  EXPECT(s.mute  == true);
  EXPECT(s.vswing_pos == 3);           // Fix mid
  EXPECT(s.fan   == FanSpeed::F1);
  EXPECT(s.power == true);
  EXPECT(s.mode  == Mode::Cool);
  EXPECT(s.setpoint_c   == 22.0f);
  EXPECT(s.indoor_temp_c == 20.0f);
  return 0;
}

int main() {
  if (test_buildQuery_matches_fixture()) return 1;
  if (test_parseState_cool_22_auto()) return 1;
  if (test_parseState_rejects_bad_checksum()) return 1;
  if (test_buildSet_heat_25_5_high()) return 1;
  if (test_eco_turbo_mute_vswing_roundtrip()) return 1;
  std::printf("OK\n");
  return 0;
}
