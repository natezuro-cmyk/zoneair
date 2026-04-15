#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../state/ac_state.h"

namespace zoneair {

// Tuya 55AA serial protocol — WiFi side.
//
// Frame format (Tuya MCU SDK, 55AA framing):
//   [0]   0x55
//   [1]   0xAA
//   [2]   version (0x00 from WiFi→MCU; MCU echoes its own value MCU→WiFi)
//   [3]   command
//   [4-5] payload length (big-endian uint16)
//   [6..] payload
//   [N-1] checksum = (sum of all preceding bytes) mod 256
//
// Commands handled:
//   0x00  Heartbeat                — MCU→WiFi query, WiFi responds w/ 1 byte
//   0x01  Product info             — MCU→WiFi query, WiFi responds w/ JSON
//   0x02  Working mode             — MCU→WiFi query, WiFi responds w/ 2 bytes
//   0x03  WiFi status              — WiFi→MCU report, 1 byte
//   0x06  Send DP command          — WiFi→MCU, payload = packed DP records
//   0x07  Report DP status         — MCU→WiFi, payload = packed DP records
//   0x08  Query DP status          — WiFi→MCU, no payload (MCU replies with 0x07)
//
// DP record encoding inside 0x06 / 0x07:
//   [0]   dpid
//   [1]   type   (0x01=bool, 0x02=int32_be, 0x03=string, 0x04=enum, 0x05=bitmap)
//   [2-3] length (big-endian uint16)
//   [4..] value

// UART send callback: implementation owns the actual Serial1 write.
using TuyaWriteFn = void (*)(const uint8_t* data, size_t len);

class TuyaProtocol {
 public:
  static constexpr uint8_t H0 = 0x55;
  static constexpr uint8_t H1 = 0xAA;

  static constexpr uint8_t CMD_HEARTBEAT      = 0x00;
  static constexpr uint8_t CMD_PRODUCT_INFO   = 0x01;
  static constexpr uint8_t CMD_WORKING_MODE   = 0x02;
  static constexpr uint8_t CMD_WIFI_STATUS    = 0x03;
  static constexpr uint8_t CMD_SET_DP         = 0x06;
  static constexpr uint8_t CMD_REPORT_DP      = 0x07;
  static constexpr uint8_t CMD_QUERY_DP       = 0x08;

  static constexpr uint8_t TYPE_BOOL   = 0x01;
  static constexpr uint8_t TYPE_VALUE  = 0x02;
  static constexpr uint8_t TYPE_STRING = 0x03;
  static constexpr uint8_t TYPE_ENUM   = 0x04;
  static constexpr uint8_t TYPE_BITMAP = 0x05;

  // DP IDs — Pioneer WYT, derived from /Users/Ben/tuya_complete_reference.md
  // PART 14 (lines 588-619) "WYT Protocol <-> Tuya DP" cross-reference.
  static constexpr uint8_t DP_POWER       = 1;   // bool   — line 594
  static constexpr uint8_t DP_SETPOINT_C  = 2;   // value  — line 596
  static constexpr uint8_t DP_INDOOR_C    = 3;   // value  — line 597
  static constexpr uint8_t DP_MODE        = 4;   // enum   — line 595, values per PART 7 (376-382)
  static constexpr uint8_t DP_FAN         = 5;   // enum   — line 598
  static constexpr uint8_t DP_ECO         = 7;   // bool   — line 599
  static constexpr uint8_t DP_TURBO       = 8;   // bool   — line 600
  static constexpr uint8_t DP_MUTE        = 9;   // bool   — line 601
  static constexpr uint8_t DP_DISPLAY     = 13;  // bool   — line 603
  static constexpr uint8_t DP_BEEP        = 16;  // bool   — line 606 (id 0x0025)
  static constexpr uint8_t DP_SETPOINT_F  = 24;  // value  — line 605 + PART 8 line 423 (id 0x0227)
  static constexpr uint8_t DP_VSWING      = 31;  // enum   — line 608 (id 0x0011)
  static constexpr uint8_t DP_TEMP_UNIT   = 8;   // bool   — see DP table line 238 (0x0013) — collides w/ turbo DPID 8.
                                                  // Pioneer firmware schema (PART 15 line 635) assigns dp_id=8 to id 0x0013.
                                                  // The "turbo" mapping in PART 14 conflicts; we treat DP 8 as bool either way.
                                                  // In practice on Pioneer this DP toggles the C/F display unit.

  // ---- low-level frame builders ----
  // Write a complete 55AA frame to `out`. Returns total bytes written, or 0 if out_cap is insufficient.
  static size_t buildFrame(uint8_t cmd, const uint8_t* payload, size_t payload_len,
                           uint8_t* out, size_t out_cap);

  static uint8_t checksum(const uint8_t* bytes, size_t n);

  // ---- high-level builders ----
  // buildSet packs a desired state into a 0x06 frame.
  // We send: power, mode, fan, setpoint_f, eco, turbo (DP8), mute, vswing.
  // Caller supplies a sufficient out buffer (>= 64 bytes).
  static size_t buildSet(const AcState& desired, uint8_t* out, size_t out_cap);
  static size_t buildQuery(uint8_t* out, size_t out_cap);          // 0x08, no payload
  static size_t buildHeartbeatAck(bool first_boot, uint8_t* out, size_t out_cap);
  static size_t buildProductInfoAck(uint8_t* out, size_t out_cap);
  static size_t buildWorkingModeAck(uint8_t* out, size_t out_cap);
  static size_t buildWifiStatus(uint8_t status, uint8_t* out, size_t out_cap);

  // ---- streaming parser ----
  // Feed UART bytes one at a time. Returns true when a complete, valid frame
  // was just consumed. The caller can then inspect lastCommand()/lastPayload().
  // Frames are silently dropped on header/length/checksum errors and the parser
  // resyncs on the next 0x55.
  bool feedByte(uint8_t b);
  uint8_t lastCommand() const { return last_cmd_; }
  const uint8_t* lastPayload() const { return payload_buf_; }
  size_t lastPayloadLen() const { return last_payload_len_; }

  // After a full frame is parsed, applyParsedFrame() updates `state` if the
  // frame is a 0x07 DP report. Returns true if state was updated.
  bool applyParsedFrame(AcState& state) const;

  // Parse a 0x07 (or 0x06) DP-record payload into AcState. Returns true if at
  // least one DP record was decoded.
  static bool parseDpPayload(const uint8_t* payload, size_t len, AcState& state);

 private:
  enum class S : uint8_t { H0, H1, VER, CMD, LEN_HI, LEN_LO, PAYLOAD, CHECK };
  S       state_           = S::H0;
  uint8_t version_         = 0;
  uint8_t cmd_             = 0;
  uint16_t expected_len_   = 0;
  uint16_t payload_idx_    = 0;
  uint8_t  running_sum_    = 0;
  uint8_t  payload_buf_[256]{};
  uint8_t  last_cmd_       = 0xFF;
  size_t   last_payload_len_ = 0;
};

}  // namespace zoneair
