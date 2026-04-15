#include "ble_prov.h"
#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "network_provisioning/manager.h"
#include "network_provisioning/scheme_ble.h"

namespace zoneair {

static BleProvisioner::OnProvisioned g_cb;
static String g_pending_slug = "unit";
static String g_pending_ssid;
static String g_pending_pass;

static void event_handler(void*, esp_event_base_t base, int32_t id, void* data) {
  if (base != NETWORK_PROV_EVENT) return;
  switch (id) {
    case NETWORK_PROV_WIFI_CRED_RECV: {
      auto* cfg = (wifi_sta_config_t*)data;
      g_pending_ssid = String((const char*)cfg->ssid);
      g_pending_pass = String((const char*)cfg->password);
      Serial.printf("[prov] creds received ssid=%s\n", g_pending_ssid.c_str());
      break;
    }
    case NETWORK_PROV_WIFI_CRED_SUCCESS:
      Serial.println("[prov] creds verified, saving + rebooting");
      if (g_cb) g_cb(g_pending_ssid, g_pending_pass, g_pending_slug);
      break;
    case NETWORK_PROV_WIFI_CRED_FAIL:
      Serial.println("[prov] creds REJECTED by AP — try again");
      break;
    case NETWORK_PROV_END:
      network_prov_mgr_deinit();
      break;
    default: break;
  }
}

static esp_err_t custom_slug_handler(uint32_t, const uint8_t* in, ssize_t in_len,
                                     uint8_t**, ssize_t*, void*) {
  g_pending_slug = String((const char*)in, in_len);
  Serial.printf("[prov] slug=%s\n", g_pending_slug.c_str());
  return ESP_OK;
}

void BleProvisioner::begin(const String& pop, OnProvisioned cb) {
  g_cb = cb;
  active_ = true;

  // Bring up the WiFi/event subsystem so network_prov_mgr can hook into it.
  WiFi.mode(WIFI_STA);

  esp_event_handler_register(NETWORK_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, nullptr);

  network_prov_mgr_config_t mgr_cfg = {};
  mgr_cfg.scheme = network_prov_scheme_ble;
  network_prov_event_handler_t handler = NETWORK_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM;
  mgr_cfg.scheme_event_handler = handler;
  network_prov_mgr_init(mgr_cfg);

  network_prov_mgr_endpoint_create("zoneair-slug");

  uint8_t mac[6]; esp_wifi_get_mac(WIFI_IF_STA, mac);
  char service_name[16];
  snprintf(service_name, sizeof(service_name), "ZoneAir-%02X%02X", mac[4], mac[5]);

  network_prov_mgr_start_provisioning(NETWORK_PROV_SECURITY_1, pop.c_str(), service_name, nullptr);
  network_prov_mgr_endpoint_register("zoneair-slug", custom_slug_handler, nullptr);

  Serial.printf("[prov] BLE advertising as %s (PoP=%s)\n", service_name, pop.c_str());
}

}
