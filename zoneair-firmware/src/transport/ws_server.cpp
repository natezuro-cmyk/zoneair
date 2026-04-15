#include "ws_server.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

namespace zoneair {

AsyncWebServer& zoneair_http_server();  // provided by http_server.cpp

static AsyncWebSocket ws("/ws");

void WsServer::begin() {
  zoneair_http_server().addHandler(&ws);
}

void WsServer::pushState(const AcState& s) {
  StaticJsonDocument<256> doc;
  doc["online"]     = s.valid;
  doc["power"]      = s.power;
  doc["mode"]       = (int)s.mode;
  doc["fan"]        = (int)s.fan;
  doc["setpoint_c"] = s.setpoint_c;
  doc["indoor_c"]   = s.indoor_temp_c;
  String body; serializeJson(doc, body);
  ws.textAll(body);
}

}
