#pragma once
#include <Arduino.h>
#include <functional>

namespace zoneair {

// Thin wrapper around Arduino-ESP32's WiFiProv library.
//
// Uses WiFiProv.beginProvision() (network_provisioning component under the hood)
// which handles WiFi low-level init, Bluedroid memory release, event loop setup,
// and the BLE scheme handler correctly. An earlier attempt called
// network_prov_mgr_init() + esp_event_handler_register() manually and crashed
// early in BLE init (LoadProhibited inside Bluedroid) because the WiFi/event
// subsystem was not fully brought up before the BLE scheme tried to register
// its GATT service. Use the Arduino wrapper and let it do the orchestration.
class BleProvisioner {
 public:
  using OnProvisioned = std::function<void(const String& ssid, const String& pass, const String& slug)>;

  // Start BLE advertising for WiFi provisioning. `cb` fires once the phone
  // app has supplied credentials AND the AP accepted them.
  void begin(const String& pop, OnProvisioned cb);

  bool isProvisioning() const { return active_; }

 private:
  bool active_ = false;
};

}
