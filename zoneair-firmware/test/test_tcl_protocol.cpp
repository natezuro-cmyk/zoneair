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

int main() {
  if (test_buildQuery_matches_fixture()) return 1;
  if (test_parseState_cool_22_auto()) return 1;
  if (test_parseState_rejects_bad_checksum()) return 1;
  std::printf("OK\n");
  return 0;
}
