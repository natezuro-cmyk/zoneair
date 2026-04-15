#pragma once
#include <Arduino.h>

namespace zoneair {
class UartLink {
 public:
  void begin(int rx_pin, int tx_pin, uint32_t baud);
  void write(const uint8_t* data, size_t len);
  size_t readWithTimeout(uint8_t* buf, size_t cap, uint32_t timeout_ms);
  void flushInput();
};
}
