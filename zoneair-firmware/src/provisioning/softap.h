#pragma once
#include <Arduino.h>
#include <functional>

namespace zoneair {

// Captive-portal SoftAP provisioning. ESP advertises an open WiFi network
// (e.g. "ZoneAir-Setup"). User joins it, the captive portal pops up the
// config form. POSTing valid creds invokes the callback.
class SoftApProvisioner {
 public:
  using OnProvisioned = std::function<void(const String& ssid, const String& pass)>;
  void begin(const String& ap_name, OnProvisioned cb);
  void poll();          // call from loop() — runs DNS server
  bool isActive() const { return active_; }
 private:
  bool active_ = false;
};

}
