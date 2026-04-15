#include "uart_link.h"

namespace zoneair {

void UartLink::begin(int rx_pin, int tx_pin, uint32_t baud) {
  Serial1.begin(baud, SERIAL_8E1, rx_pin, tx_pin);
  while (Serial1.available()) Serial1.read();
}

void UartLink::write(const uint8_t* data, size_t len) {
  Serial1.write(data, len);
  Serial1.flush();
}

size_t UartLink::readWithTimeout(uint8_t* buf, size_t cap, uint32_t timeout_ms) {
  size_t n = 0;
  uint32_t deadline = millis() + timeout_ms;
  while (n < cap && (int32_t)(deadline - millis()) > 0) {
    if (Serial1.available()) {
      buf[n++] = Serial1.read();
    } else {
      delay(1);
    }
  }
  return n;
}

void UartLink::flushInput() {
  while (Serial1.available()) Serial1.read();
}

}
