#include <Arduino.h>
#include <WiFi.h>
#include "src/state/ac_state.h"
#include "src/protocol/tcl.h"
#include "src/uart_link.h"
#include "src/transport/http_server.h"
#include "src/transport/ws_server.h"
#include "src/discovery/mdns.h"

using namespace zoneair;

static const char* WIFI_SSID = "REPLACE_WITH_HOME_SSID";
static const char* WIFI_PASS = "REPLACE_WITH_HOME_PASS";

static constexpr int RX_PIN = 44;  // SuperMini silkscreen "RX" — wired to AC's TX
static constexpr int TX_PIN = 43;  // SuperMini silkscreen "TX" — wired to AC's RX
static constexpr uint32_t TCL_BAUD = 9600;
static constexpr uint32_t POLL_INTERVAL_MS = 3000;
static constexpr uint32_t QUERY_TIMEOUT_MS = 300;

UartLink uart;
AcState  ac{};
HttpServer http;
WsServer ws;
static const char* UNIT_SLUG = "test";

static void sendSet(const AcState& desired) {
  uint8_t sbuf[64];
  size_t n = TclProtocol::buildSet(desired, sbuf, sizeof(sbuf));
  if (n == 0) { Serial.println("[set] encode failed"); return; }
  uart.flushInput();
  uart.write(sbuf, n);
  Serial.printf("[set] sent %u bytes\n", (unsigned)n);
}

static void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("[wifi] connecting to %s\n", WIFI_SSID);
  uint32_t deadline = millis() + 30000;
  while (WiFi.status() != WL_CONNECTED && (int32_t)(deadline - millis()) > 0) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[wifi] ip=%s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[wifi] FAILED");
  }
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

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("[zoneair] boot");
  uart.begin(RX_PIN, TX_PIN, TCL_BAUD);
  connectWifi();
  http.begin(&ac, sendSet);
  Serial.println("[http] started on port 80");
  ws.begin();
  startMdns(UNIT_SLUG);
}

void loop() {
  static uint32_t next = 0;
  if ((int32_t)(millis() - next) >= 0) {
    next = millis() + POLL_INTERVAL_MS;
    pollOnce();
  }
  delay(10);
}
