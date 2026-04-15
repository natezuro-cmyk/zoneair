// Native g++ tests for src/protocol/tuya.{h,cpp}.
//
// Coverage:
//   - checksum + frame builder
//   - heartbeat ack (first boot vs subsequent)
//   - 0x07 DP report parse: power, mode, fan, setpoint_f, indoor_c
//   - buildSet round-trip: encode → re-parse via streaming feedByte → fields match
//   - rejects bad checksum
#include <cstdio>
#include <cstring>
#include <cstdint>
#include "../src/protocol/tuya.h"

using namespace zoneair;

#define EXPECT(cond) do { if (!(cond)) { \
  std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
  return 1; } } while(0)

static int test_checksum_and_buildFrame() {
  uint8_t out[16];
  // Empty payload heartbeat-style frame: 55 AA 00 08 00 00 + checksum.
  size_t n = TuyaProtocol::buildFrame(0x08, nullptr, 0, out, sizeof(out));
  EXPECT(n == 7);
  EXPECT(out[0] == 0x55);
  EXPECT(out[1] == 0xAA);
  EXPECT(out[2] == 0x00);
  EXPECT(out[3] == 0x08);
  EXPECT(out[4] == 0x00);
  EXPECT(out[5] == 0x00);
  uint8_t want = (0x55 + 0xAA + 0x00 + 0x08 + 0x00 + 0x00) & 0xFF;
  EXPECT(out[6] == want);
  return 0;
}

static int test_buildHeartbeatAck() {
  uint8_t buf[16];
  size_t n = TuyaProtocol::buildHeartbeatAck(true, buf, sizeof(buf));
  EXPECT(n == 8);          // 7 header + 1 byte payload
  EXPECT(buf[3] == 0x00);  // CMD_HEARTBEAT
  EXPECT(buf[5] == 0x01);  // length lo
  EXPECT(buf[6] == 0x00);  // first-boot byte

  size_t n2 = TuyaProtocol::buildHeartbeatAck(false, buf, sizeof(buf));
  EXPECT(n2 == 8);
  EXPECT(buf[6] == 0x01);
  return 0;
}

// Synthesize a valid 0x07 report frame containing:
//   DP1 power=1, DP4 mode=Cool(0), DP5 fan=Auto(0),
//   DP24 setpoint_f=72, DP3 indoor=75 (we treat as °C raw value)
static size_t make_report_frame(uint8_t* out, size_t cap) {
  uint8_t pl[64];
  size_t i = 0;
  // DP1 bool 1
  pl[i++] = 1;  pl[i++] = 0x01; pl[i++] = 0; pl[i++] = 1; pl[i++] = 1;
  // DP4 enum 0 (cool)
  pl[i++] = 4;  pl[i++] = 0x04; pl[i++] = 0; pl[i++] = 1; pl[i++] = 0;
  // DP5 enum 0 (auto)
  pl[i++] = 5;  pl[i++] = 0x04; pl[i++] = 0; pl[i++] = 1; pl[i++] = 0;
  // DP24 value 72 (Fahrenheit, int32 BE)
  pl[i++] = 24; pl[i++] = 0x02; pl[i++] = 0; pl[i++] = 4;
  pl[i++] = 0; pl[i++] = 0; pl[i++] = 0; pl[i++] = 72;
  // DP3 value 75 (indoor — we read as °C in our model; in PR we'll empirically
  // confirm whether Pioneer scales °Cx10).
  pl[i++] = 3;  pl[i++] = 0x02; pl[i++] = 0; pl[i++] = 4;
  pl[i++] = 0; pl[i++] = 0; pl[i++] = 0; pl[i++] = 75;

  return TuyaProtocol::buildFrame(TuyaProtocol::CMD_REPORT_DP, pl, i, out, cap);
}

static int test_parseDpPayload_via_feedByte() {
  uint8_t frame[128];
  size_t n = make_report_frame(frame, sizeof(frame));
  EXPECT(n > 0);

  TuyaProtocol p;
  bool got = false;
  for (size_t i = 0; i < n; ++i) {
    if (p.feedByte(frame[i])) { got = true; }
  }
  EXPECT(got);
  EXPECT(p.lastCommand() == TuyaProtocol::CMD_REPORT_DP);

  AcState s{};
  EXPECT(p.applyParsedFrame(s));
  EXPECT(s.valid);
  EXPECT(s.power == true);
  EXPECT(s.mode == Mode::Cool);
  EXPECT(s.fan == FanSpeed::Auto);
  EXPECT(s.setpoint_f == 72);
  EXPECT(s.indoor_temp_c == 75.0f);
  return 0;
}

static int test_buildSet_roundtrip() {
  AcState desired{};
  desired.power = true;
  desired.mode = Mode::Heat;
  desired.fan = FanSpeed::F3;     // → wire 2
  desired.setpoint_f = 68;
  desired.eco = false;
  desired.turbo = true;
  desired.mute = false;
  desired.vswing_pos = 3;          // → wire 4 (mid)

  uint8_t buf[128];
  size_t n = TuyaProtocol::buildSet(desired, buf, sizeof(buf));
  EXPECT(n > 7);

  // Verify checksum.
  uint8_t want = TuyaProtocol::checksum(buf, n - 1);
  EXPECT(buf[n - 1] == want);

  // Stream-parse it back.
  TuyaProtocol p;
  bool got = false;
  for (size_t i = 0; i < n; ++i) if (p.feedByte(buf[i])) got = true;
  EXPECT(got);
  EXPECT(p.lastCommand() == TuyaProtocol::CMD_SET_DP);

  AcState parsed{};
  EXPECT(p.applyParsedFrame(parsed));
  EXPECT(parsed.power == true);
  EXPECT(parsed.mode == Mode::Heat);
  EXPECT(parsed.fan == FanSpeed::F3);
  EXPECT(parsed.setpoint_f == 68);
  EXPECT(parsed.turbo == true);
  EXPECT(parsed.eco == false);
  EXPECT(parsed.vswing_pos == 3);
  return 0;
}

static int test_rejects_bad_checksum() {
  uint8_t frame[128];
  size_t n = make_report_frame(frame, sizeof(frame));
  EXPECT(n > 0);
  frame[n - 1] ^= 0xFF;  // corrupt checksum

  TuyaProtocol p;
  bool got = false;
  for (size_t i = 0; i < n; ++i) if (p.feedByte(frame[i])) got = true;
  EXPECT(!got);
  return 0;
}

static int test_resync_after_garbage() {
  uint8_t frame[128];
  size_t n = make_report_frame(frame, sizeof(frame));
  EXPECT(n > 0);

  TuyaProtocol p;
  // Feed garbage first.
  uint8_t garbage[] = { 0x00, 0xFF, 0x12, 0x55, 0x99, 0xAA, 0x55 };
  for (uint8_t b : garbage) p.feedByte(b);
  bool got = false;
  for (size_t i = 0; i < n; ++i) if (p.feedByte(frame[i])) got = true;
  EXPECT(got);
  return 0;
}

int main() {
  if (test_checksum_and_buildFrame())        return 1;
  if (test_buildHeartbeatAck())              return 1;
  if (test_parseDpPayload_via_feedByte())    return 1;
  if (test_buildSet_roundtrip())             return 1;
  if (test_rejects_bad_checksum())           return 1;
  if (test_resync_after_garbage())           return 1;
  std::printf("OK\n");
  return 0;
}
