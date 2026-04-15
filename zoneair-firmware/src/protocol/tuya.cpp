#include "tuya.h"
#include <cstring>

namespace zoneair {

uint8_t TuyaProtocol::checksum(const uint8_t* bytes, size_t n) {
  uint32_t s = 0;
  for (size_t i = 0; i < n; ++i) s += bytes[i];
  return static_cast<uint8_t>(s & 0xFF);
}

size_t TuyaProtocol::buildFrame(uint8_t cmd, const uint8_t* payload, size_t payload_len,
                                uint8_t* out, size_t out_cap) {
  // Header(2) + version(1) + cmd(1) + len(2) + payload + checksum(1)
  size_t total = 7 + payload_len;
  if (out_cap < total) return 0;
  out[0] = H0;
  out[1] = H1;
  out[2] = 0x00;                                 // version: WiFi → MCU is always 0
  out[3] = cmd;
  out[4] = static_cast<uint8_t>((payload_len >> 8) & 0xFF);
  out[5] = static_cast<uint8_t>(payload_len & 0xFF);
  if (payload_len > 0 && payload != nullptr) {
    std::memcpy(out + 6, payload, payload_len);
  }
  out[total - 1] = checksum(out, total - 1);
  return total;
}

// Helpers to append a single DP record into a payload buffer.
// Returns new offset, or 0 on overflow.
static size_t appendDpBool(uint8_t dpid, bool v, uint8_t* buf, size_t off, size_t cap) {
  if (off + 5 > cap) return 0;
  buf[off++] = dpid;
  buf[off++] = TuyaProtocol::TYPE_BOOL;
  buf[off++] = 0x00;
  buf[off++] = 0x01;
  buf[off++] = v ? 1 : 0;
  return off;
}

static size_t appendDpEnum(uint8_t dpid, uint8_t v, uint8_t* buf, size_t off, size_t cap) {
  if (off + 5 > cap) return 0;
  buf[off++] = dpid;
  buf[off++] = TuyaProtocol::TYPE_ENUM;
  buf[off++] = 0x00;
  buf[off++] = 0x01;
  buf[off++] = v;
  return off;
}

static size_t appendDpValue(uint8_t dpid, int32_t v, uint8_t* buf, size_t off, size_t cap) {
  if (off + 8 > cap) return 0;
  buf[off++] = dpid;
  buf[off++] = TuyaProtocol::TYPE_VALUE;
  buf[off++] = 0x00;
  buf[off++] = 0x04;
  buf[off++] = static_cast<uint8_t>((v >> 24) & 0xFF);
  buf[off++] = static_cast<uint8_t>((v >> 16) & 0xFF);
  buf[off++] = static_cast<uint8_t>((v >> 8)  & 0xFF);
  buf[off++] = static_cast<uint8_t>( v        & 0xFF);
  return off;
}

// Map our internal Mode enum to the wire value for DP 4.
// Wire: 0=Cool, 1=Heat, 2=Fan, 3=Dry, 4=Auto.
// Source: /Users/Ben/tuya_complete_reference.md PART 7 lines 376-382.
static uint8_t modeToWire(Mode m) {
  switch (m) {
    case Mode::Cool: return 0;
    case Mode::Heat: return 1;
    case Mode::Fan:  return 2;
    case Mode::Dry:  return 3;
    case Mode::Auto: return 4;
    case Mode::Off:  // off is sent as power=0, mode unchanged; default Cool
    default:         return 0;
  }
}

static Mode wireToMode(uint8_t v) {
  switch (v) {
    case 0: return Mode::Cool;
    case 1: return Mode::Heat;
    case 2: return Mode::Fan;
    case 3: return Mode::Dry;
    case 4: return Mode::Auto;
    default: return Mode::Cool;
  }
}

// Map FanSpeed to DP 5 enum value.
// Pioneer/Daizuki convention (best-guess until empirically verified):
//   Auto=0, Low=1, Medium=2, High=3
// We collapse F1/F2 → Low, F3 → Medium, F4/F5 → High.
static uint8_t fanToWire(FanSpeed f) {
  switch (f) {
    case FanSpeed::Auto: return 0;
    case FanSpeed::F1:   return 1;
    case FanSpeed::F2:   return 1;
    case FanSpeed::F3:   return 2;
    case FanSpeed::F4:   return 3;
    case FanSpeed::F5:   return 3;
    default:             return 0;
  }
}

static FanSpeed wireToFan(uint8_t v) {
  switch (v) {
    case 0: return FanSpeed::Auto;
    case 1: return FanSpeed::F1;
    case 2: return FanSpeed::F3;
    case 3: return FanSpeed::F5;
    default: return FanSpeed::Auto;
  }
}

// Map our 0..8 vswing index to a single DP 31 enum byte.
// Tuya commonly uses: 0=off/closed, 1=full-swing, 2=top, 3=upper, 4=middle,
// 5=lower, 6=bottom. We map best-effort:
//   0 → 0 (off / no change), 1..5 → 2..6 (fixed top..bottom),
//   6 → 1 (full sweep), 7 → 3 (upper), 8 → 5 (lower).
static uint8_t vswingToWire(uint8_t pos) {
  switch (pos) {
    case 0: return 0;
    case 1: return 2;
    case 2: return 3;
    case 3: return 4;
    case 4: return 5;
    case 5: return 6;
    case 6: return 1;
    case 7: return 3;
    case 8: return 5;
    default: return 0;
  }
}

static uint8_t wireToVswing(uint8_t v) {
  switch (v) {
    case 0: return 0;
    case 1: return 6;  // full sweep
    case 2: return 1;  // top
    case 3: return 2;  // upper
    case 4: return 3;  // mid
    case 5: return 4;  // lower
    case 6: return 5;  // bottom
    default: return 0;
  }
}

size_t TuyaProtocol::buildSet(const AcState& d, uint8_t* out, size_t out_cap) {
  uint8_t pl[64];
  size_t off = 0;
  off = appendDpBool (DP_POWER,      d.power,                     pl, off, sizeof(pl));
  if (d.power) {
    off = appendDpEnum (DP_MODE,     modeToWire(d.mode),          pl, off, sizeof(pl));
    off = appendDpEnum (DP_FAN,      fanToWire(d.fan),            pl, off, sizeof(pl));
    int sf = d.setpoint_f;
    if (sf < 61) sf = 61;
    if (sf > 88) sf = 88;
    off = appendDpValue(DP_SETPOINT_F, sf,                        pl, off, sizeof(pl));
    off = appendDpBool (DP_ECO,      d.eco,                       pl, off, sizeof(pl));
    off = appendDpBool (DP_TURBO,    d.turbo,                     pl, off, sizeof(pl));
    off = appendDpBool (DP_MUTE,     d.mute,                      pl, off, sizeof(pl));
    off = appendDpEnum (DP_VSWING,   vswingToWire(d.vswing_pos),  pl, off, sizeof(pl));
  }
  if (off == 0) return 0;
  return buildFrame(CMD_SET_DP, pl, off, out, out_cap);
}

size_t TuyaProtocol::buildQuery(uint8_t* out, size_t out_cap) {
  return buildFrame(CMD_QUERY_DP, nullptr, 0, out, out_cap);
}

size_t TuyaProtocol::buildHeartbeatAck(bool first_boot, uint8_t* out, size_t out_cap) {
  uint8_t b = first_boot ? 0x00 : 0x01;
  return buildFrame(CMD_HEARTBEAT, &b, 1, out, out_cap);
}

size_t TuyaProtocol::buildProductInfoAck(uint8_t* out, size_t out_cap) {
  // Minimal Pioneer-flavored product info JSON. Real WBR3 dongle sends a
  // similar shape (see /Users/Ben/tuya_complete_reference.md PART 15 line 627
  // "mcu":[{"id":"V9-RABMT26AC-F"...).
  static const char* pid = "{\"p\":\"zoneair_pioneer\",\"v\":\"1.0.0\"}";
  return buildFrame(CMD_PRODUCT_INFO, reinterpret_cast<const uint8_t*>(pid),
                    std::strlen(pid), out, out_cap);
}

size_t TuyaProtocol::buildWorkingModeAck(uint8_t* out, size_t out_cap) {
  // Two bytes: WiFi-status indicator GPIO, network-reset GPIO.
  // 0xFF/0xFF means "module handles networking itself — no MCU LEDs to drive".
  uint8_t pl[2] = { 0xFF, 0xFF };
  return buildFrame(CMD_WORKING_MODE, pl, 2, out, out_cap);
}

size_t TuyaProtocol::buildWifiStatus(uint8_t status, uint8_t* out, size_t out_cap) {
  return buildFrame(CMD_WIFI_STATUS, &status, 1, out, out_cap);
}

bool TuyaProtocol::parseDpPayload(const uint8_t* payload, size_t len, AcState& state) {
  size_t i = 0;
  bool any = false;
  while (i + 4 <= len) {
    uint8_t  dpid = payload[i + 0];
    uint8_t  type = payload[i + 1];
    uint16_t dlen = (static_cast<uint16_t>(payload[i + 2]) << 8) | payload[i + 3];
    if (i + 4 + dlen > len) break;
    const uint8_t* v = payload + i + 4;

    switch (dpid) {
      case DP_POWER:
        if (type == TYPE_BOOL && dlen == 1) state.power = (v[0] != 0);
        any = true;
        break;
      case DP_SETPOINT_F:
        if (type == TYPE_VALUE && dlen == 4) {
          int32_t x = (int32_t)((uint32_t)v[0] << 24 | (uint32_t)v[1] << 16 |
                                (uint32_t)v[2] << 8  | (uint32_t)v[3]);
          state.setpoint_f = static_cast<int>(x);
        }
        any = true;
        break;
      case DP_INDOOR_C:
        if (type == TYPE_VALUE && dlen == 4) {
          int32_t x = (int32_t)((uint32_t)v[0] << 24 | (uint32_t)v[1] << 16 |
                                (uint32_t)v[2] << 8  | (uint32_t)v[3]);
          // Pioneer reports indoor temp in Celsius — usually scaled by 1.
          // If your unit reports x10, you'll see values like 220 instead of 22 —
          // adjust here once captured on hardware.
          state.indoor_temp_c = static_cast<float>(x);
        }
        any = true;
        break;
      case DP_MODE:
        if (type == TYPE_ENUM && dlen >= 1) state.mode = wireToMode(v[0]);
        any = true;
        break;
      case DP_FAN:
        if (type == TYPE_ENUM && dlen >= 1) state.fan = wireToFan(v[0]);
        any = true;
        break;
      case DP_ECO:
        if (type == TYPE_BOOL && dlen == 1) state.eco = (v[0] != 0);
        any = true;
        break;
      case DP_TURBO:
        if (type == TYPE_BOOL && dlen == 1) state.turbo = (v[0] != 0);
        any = true;
        break;
      case DP_MUTE:
        if (type == TYPE_BOOL && dlen == 1) state.mute = (v[0] != 0);
        any = true;
        break;
      case DP_VSWING:
        if (type == TYPE_ENUM && dlen >= 1) state.vswing_pos = wireToVswing(v[0]);
        any = true;
        break;
      default:
        // Unknown DP — ignore but advance.
        break;
    }
    i += 4 + dlen;
  }
  if (any) state.valid = true;
  return any;
}

bool TuyaProtocol::feedByte(uint8_t b) {
  switch (state_) {
    case S::H0:
      if (b == H0) { running_sum_ = b; state_ = S::H1; }
      return false;
    case S::H1:
      if (b == H1) { running_sum_ += b; state_ = S::VER; }
      else { state_ = (b == H0) ? S::H1 : S::H0; running_sum_ = (b == H0) ? b : 0; }
      return false;
    case S::VER:
      version_ = b; running_sum_ += b; state_ = S::CMD;
      return false;
    case S::CMD:
      cmd_ = b; running_sum_ += b; state_ = S::LEN_HI;
      return false;
    case S::LEN_HI:
      expected_len_ = (uint16_t)b << 8;
      running_sum_ += b;
      state_ = S::LEN_LO;
      return false;
    case S::LEN_LO:
      expected_len_ |= b;
      running_sum_ += b;
      payload_idx_ = 0;
      if (expected_len_ > sizeof(payload_buf_)) {
        // payload too large — abort & resync
        state_ = S::H0;
        return false;
      }
      state_ = (expected_len_ == 0) ? S::CHECK : S::PAYLOAD;
      return false;
    case S::PAYLOAD:
      payload_buf_[payload_idx_++] = b;
      running_sum_ += b;
      if (payload_idx_ >= expected_len_) state_ = S::CHECK;
      return false;
    case S::CHECK: {
      uint8_t want = running_sum_ & 0xFF;
      state_ = S::H0;
      if (b != want) return false;
      last_cmd_ = cmd_;
      last_payload_len_ = expected_len_;
      return true;
    }
  }
  return false;
}

bool TuyaProtocol::applyParsedFrame(AcState& state) const {
  if (last_cmd_ != CMD_REPORT_DP && last_cmd_ != CMD_SET_DP) return false;
  return parseDpPayload(payload_buf_, last_payload_len_, state);
}

}  // namespace zoneair
