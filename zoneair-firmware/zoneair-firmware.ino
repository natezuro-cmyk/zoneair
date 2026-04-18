#include <Arduino.h>
#include <WiFi.h>
#include <esp_mac.h>
#include "src/state/ac_state.h"
#include "src/protocol/tcl.h"
#include "src/uart_link.h"
#include "src/transport/http_server.h"
#include "src/transport/ws_server.h"
#include "src/discovery/mdns.h"
#include "src/state/nvs_store.h"
#include "src/provisioning/softap.h"

using namespace zoneair;

static constexpr int RX_PIN = 44;  // SuperMini silkscreen "RX" — wired to AC's TX
static constexpr int TX_PIN = 43;  // SuperMini silkscreen "TX" — wired to AC's RX
static constexpr uint32_t TCL_BAUD = 9600;
static constexpr uint32_t POLL_INTERVAL_MS = 3000;
static constexpr uint32_t QUERY_TIMEOUT_MS = 300;

UartLink uart;
AcState  ac{};
HttpServer http;
WsServer ws;
NvsStore nvs;
SoftApProvisioner prov;
static String unit_slug = "unit";

static void sendSet(const AcState& desired) {
  uint8_t sbuf[64];
  size_t n = TclProtocol::buildSet(desired, sbuf, sizeof(sbuf));
  if (n == 0) { Serial.println("[set] encode failed"); return; }
  uart.flushInput();
  uart.write(sbuf, n);
  Serial.printf("[set] sent %u bytes\n", (unsigned)n);
  // Commit write-only fields (AC doesn't echo these in its response, so the
  // parser ignores them — stash here so the next /state poll returns truth).
  ac.beep    = desired.beep;
  ac.display = desired.display;
}

static bool connectWifi(const String& ssid, const String& pass) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.printf("[wifi] connecting to %s\n", ssid.c_str());
  uint32_t deadline = millis() + 30000;
  while (WiFi.status() != WL_CONNECTED && (int32_t)(deadline - millis()) > 0) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[wifi] ip=%s\n", WiFi.localIP().toString().c_str());
    return true;
  }
  Serial.println("[wifi] FAILED");
  return false;
}

static void pollOnce() {
  uint8_t qbuf[16];
  size_t qn = TclProtocol::buildQuery(qbuf, sizeof(qbuf));
  uart.flushInput();
  uart.write(qbuf, qn);
  uint8_t rbuf[128];
  size_t rn = uart.readWithTimeout(rbuf, sizeof(rbuf), QUERY_TIMEOUT_MS);
  if (rn == 0) { Serial.println("[poll] no response"); return; }
  if (TclProtocol::parseState(rbuf, rn, ac)) {
    Serial.printf("[poll] mode=%d set=%.1f indoor=%.1f power=%d fan=%d\n",
      (int)ac.mode, ac.setpoint_c, ac.indoor_temp_c, ac.power, (int)ac.fan);
    ws.pushState(ac);
  } else {
    Serial.printf("[poll] parse fail (%u bytes)\n", (unsigned)rn);
  }
}

// Boot button on GPIO 0 — long-press 5s to factory-reset (clear NVS + reboot).
static constexpr int BOOT_BUTTON = 0;
static constexpr uint32_t RESET_HOLD_MS = 5000;
static uint32_t button_down_at = 0;

static void factoryReset(const char* reason) {
  Serial.printf("[reset] %s — clearing NVS and rebooting\n", reason);
  nvs.clear();
  delay(300);
  ESP.restart();
}

static void checkResetButton() {
  if (digitalRead(BOOT_BUTTON) == LOW) {
    if (button_down_at == 0) button_down_at = millis();
    if (millis() - button_down_at > RESET_HOLD_MS) factoryReset("boot-button long press");
  } else {
    button_down_at = 0;
  }
}

static bool services_started = false;
static void startServices(const String& slug) {
  http.begin(&ac, sendSet);
  Serial.println("[http] started on port 80");
  ws.begin();
  startMdns(slug.c_str());
  services_started = true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("[zoneair] boot");
  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  uart.begin(RX_PIN, TX_PIN, TCL_BAUD);

  auto cfg = nvs.load();
  if (!cfg.valid) {
    Serial.println("[zoneair] no creds — starting SoftAP captive portal");
    // Append last 4 MAC hex to SSID — defeats iOS captive cache and lets
    // multiple units provision side-by-side.
    uint8_t mac[6]; esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char ap_name[24];
    snprintf(ap_name, sizeof(ap_name), "Z1Air-Setup-%02X%02X", mac[4], mac[5]);
    prov.begin(ap_name, [](const String& ssid, const String& pass){
      NvsStore s; s.save(ssid, pass, "unit");
      Serial.println("[prov] saved, rebooting in 2s");
      delay(2000);
      ESP.restart();
    });
    return;
  }
  if (!connectWifi(cfg.ssid, cfg.pass)) {
    factoryReset("WiFi connect failed");
  }
  unit_slug = cfg.slug.length() > 0 ? cfg.slug : String("unit");
  startServices(unit_slug);
}

void loop() {
  checkResetButton();

  if (prov.isActive()) {
    prov.poll();
    delay(10);
    return;
  }

  static uint32_t next = 0;
  if ((int32_t)(millis() - next) >= 0) {
    next = millis() + POLL_INTERVAL_MS;
    pollOnce();
  }
  delay(10);
}
