#pragma once
#include <Arduino.h>
#include <functional>

namespace zoneair {

class BleProvisioner {
 public:
  using OnProvisioned = std::function<void(const String& ssid, const String& pass, const String& slug)>;
  void begin(const String& pop, OnProvisioned cb);
  bool isProvisioning() const { return active_; }
 private:
  bool active_ = false;
};

}
