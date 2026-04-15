#include "ble_prov.h"

#include <WiFi.h>
#include <WiFiProv.h>
#include <esp_wifi.h>

namespace zoneair {

// File-scope state for the event callback (the WiFiProv arduino event hook is
// a C-style function pointer, so we keep the user callback + creds here).
static BleProvisioner::OnProvisioned g_cb;
static String g_pending_ssid;
static String g_pending_pass;
static const String g_default_slug = "unit";

static void ProvEventHandler(arduino_event_t* ev) {
  switch (ev->event_id) {
    case ARDUINO_EVENT_PROV_START:
      Serial.println("[prov] BLE advertising started");
      break;

    case ARDUINO_EVENT_PROV_CRED_RECV: {
      g_pending_ssid = String((const char*)ev->event_info.prov_cred_recv.ssid);
      g_pending_pass = String((const char*)ev->event_info.prov_cred_recv.password);
      Serial.printf("[prov] creds received ssid=%s\n", g_pending_ssid.c_str());
      break;
    }

    case ARDUINO_EVENT_PROV_CRED_FAIL:
      Serial.println("[prov] creds REJECTED by AP — retry from app");
      break;

    case ARDUINO_EVENT_PROV_CRED_SUCCESS:
      Serial.println("[prov] creds verified");
      if (g_cb) g_cb(g_pending_ssid, g_pending_pass, g_default_slug);
      break;

    case ARDUINO_EVENT_PROV_END:
      Serial.println("[prov] ended");
      break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("[prov] got ip=%s\n", IPAddress(ev->event_info.got_ip.ip_info.ip.addr).toString().c_str());
      break;

    default:
      break;
  }
}

void BleProvisioner::begin(const String& pop, OnProvisioned cb) {
  g_cb = cb;
  active_ = true;

  // Register arduino-level event hook BEFORE beginProvision so we catch
  // PROV_START and everything after.
  WiFi.onEvent(ProvEventHandler);

  // WiFi.begin() with no args triggers the Arduino WiFi stack init without
  // attempting to join anything — needed so WiFiProv can attach its event
  // handlers. The example sketch does this exact dance.
  WiFi.begin();

  // Derive a short, unique service name from the MAC so multiple units
  // provision side-by-side without collision.
  uint8_t mac[6] = {0};
  esp_wifi_get_mac(WIFI_IF_STA, mac);
  static char service_name[20];
  snprintf(service_name, sizeof(service_name), "ZoneAir-%02X%02X", mac[4], mac[5]);

  Serial.printf("[prov] starting BLE provisioning as %s (PoP=%s)\n", service_name, pop.c_str());

  // FREE_BTDM releases classic BT memory after provisioning so we reclaim RAM
  // once the user has paired. reset_provisioned=false keeps whatever IDF-side
  // saved creds exist (our NvsStore is separate and authoritative).
  WiFiProv.beginProvision(
      NETWORK_PROV_SCHEME_BLE,
      NETWORK_PROV_SCHEME_HANDLER_FREE_BTDM,
      NETWORK_PROV_SECURITY_1,
      pop.c_str(),
      service_name,
      /*service_key=*/nullptr,
      /*uuid=*/nullptr,
      /*reset_provisioned=*/false);

  // Print a QR code to serial so the provisioning app on the phone can scan it.
  WiFiProv.printQR(service_name, pop.c_str(), "ble");
}

}
