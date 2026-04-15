#include <Arduino.h>
#include <WiFi.h>
#include "src/state/ac_state.h"
#include "src/protocol/tuya.h"
#include "src/uart_link.h"
#include "src/transport/http_server.h"
#include "src/transport/ws_server.h"
#include "src/discovery/mdns.h"
#include "src/state/nvs_store.h"
#include "src/provisioning/ble_prov.h"

using namespace zoneair;

static constexpr int RX_PIN = 44;  // SuperMini silkscreen "RX" — wired to AC's TX
static constexpr int TX_PIN = 43;  // SuperMini silkscreen "TX" — wired to AC's RX
static constexpr uint32_t TUYA_BAUD = 38400;
static constexpr uint32_t QUERY_INTERVAL_MS     = 3000;
static constexpr uint32_t HEARTBEAT_INTERVAL_MS = 15000;

UartLink       uart;
AcState        ac{};
HttpServer     http;
WsServer       ws;
NvsStore       nvs;
BleProvisioner prov;
TuyaProtocol   tuya;
static String  unit_slug = "unit";
static bool    first_heartbeat = true;

static void sendSet(const AcState& desired) {
  uint8_t sbuf[96];
  size_t n = TuyaProtocol::buildSet(desired, sbuf, sizeof(sbuf));
  if (n == 0) { Serial.println("[set] encode failed"); return; }
  uart.write(sbuf, n);
  Serial.printf("[set] sent %u bytes\n", (unsigned)n);
}

static void sendQuery() {
  uint8_t qbuf[16];
  size_t n = TuyaProtocol::buildQuery(qbuf, sizeof(qbuf));
  uart.write(qbuf, n);
}

static void sendHeartbeat() {
  uint8_t hbuf[16];
  size_t n = TuyaProtocol::buildHeartbeatAck(first_heartbeat, hbuf, sizeof(hbuf));
  uart.write(hbuf, n);
  first_heartbeat = false;
}

static void sendWifiConnected() {
  uint8_t wbuf[16];
  // 4 = "connected to internet" in Tuya MCU SDK conventions.
  size_t n = TuyaProtocol::buildWifiStatus(0x04, wbuf, sizeof(wbuf));
  uart.write(wbuf, n);
}

// Drain the UART, feed the streaming parser, react to every full frame.
static void pumpUart() {
  uint8_t rbuf[64];
  size_t n = uart.readWithTimeout(rbuf, sizeof(rbuf), 0);
  for (size_t i = 0; i < n; ++i) {
    if (!tuya.feedByte(rbuf[i])) continue;
    uint8_t cmd = tuya.lastCommand();
    switch (cmd) {
      case TuyaProtocol::CMD_HEARTBEAT: {
        // MCU-initiated heartbeat poll — respond.
        uint8_t buf[16];
        size_t m = TuyaProtocol::buildHeartbeatAck(first_heartbeat, buf, sizeof(buf));
        uart.write(buf, m);
        first_heartbeat = false;
        break;
      }
      case TuyaProtocol::CMD_PRODUCT_INFO: {
        uint8_t buf[64];
        size_t m = TuyaProtocol::buildProductInfoAck(buf, sizeof(buf));
        uart.write(buf, m);
        break;
      }
      case TuyaProtocol::CMD_WORKING_MODE: {
        uint8_t buf[16];
        size_t m = TuyaProtocol::buildWorkingModeAck(buf, sizeof(buf));
        uart.write(buf, m);
        // After we finish the init handshake the MCU expects a wifi-status push.
        sendWifiConnected();
        // …and then it's customary to ask for the current DP dump.
        sendQuery();
        break;
      }
      case TuyaProtocol::CMD_REPORT_DP:
      case TuyaProtocol::CMD_SET_DP: {
        if (tuya.applyParsedFrame(ac)) {
          Serial.printf("[poll] mode=%d setF=%d indoorC=%.1f power=%d fan=%d\n",
            (int)ac.mode, ac.setpoint_f, ac.indoor_temp_c, ac.power, (int)ac.fan);
          ws.pushState(ac);
        }
        break;
      }
      default:
        Serial.printf("[uart] cmd=0x%02X len=%u\n", cmd, (unsigned)tuya.lastPayloadLen());
        break;
    }
  }
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

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("[zoneair] boot");
  uart.begin(RX_PIN, TX_PIN, TUYA_BAUD);

  auto cfg = nvs.load();
  if (!cfg.valid) {
    Serial.println("[zoneair] no creds — entering BLE provisioning");
    prov.begin("ZONEAIR123", [](const String& ssid, const String& pass, const String& slug){
      NvsStore s;
      s.save(ssid, pass, slug);
      Serial.println("[prov] saved, restarting");
      delay(1500);
      ESP.restart();
    });
    return;
  }

  if (!connectWifi(cfg.ssid, cfg.pass)) {
    Serial.println("[zoneair] wifi failed — clearing NVS to re-provision on next boot");
    nvs.clear();
    delay(500);
    ESP.restart();
  }
  unit_slug = cfg.slug.length() > 0 ? cfg.slug : String("unit");

  http.begin(&ac, sendSet);
  Serial.println("[http] started on port 80");
  ws.begin();
  startMdns(unit_slug.c_str());

  // Kick off the Tuya MCU handshake: heartbeat first, then let the MCU drive.
  sendHeartbeat();
  sendWifiConnected();
  sendQuery();
}

static bool services_started = false;

void loop() {
  if (prov.isProvisioning() && !services_started && WiFi.status() == WL_CONNECTED) {
    Serial.printf("[wifi] ip=%s\n", WiFi.localIP().toString().c_str());
    http.begin(&ac, sendSet);
    Serial.println("[http] started on port 80");
    ws.begin();
    startMdns(unit_slug.c_str());
    sendHeartbeat();
    sendWifiConnected();
    sendQuery();
    services_started = true;
  }
  if (prov.isProvisioning() && !services_started) { delay(50); return; }

  // Pump UART continuously so we ack heartbeats / collect 0x07 reports.
  pumpUart();

  static uint32_t next_q = 0;
  if ((int32_t)(millis() - next_q) >= 0) {
    next_q = millis() + QUERY_INTERVAL_MS;
    sendQuery();
  }

  static uint32_t next_h = HEARTBEAT_INTERVAL_MS;
  if ((int32_t)(millis() - next_h) >= 0) {
    next_h = millis() + HEARTBEAT_INTERVAL_MS;
    sendHeartbeat();
  }

  delay(5);
}
