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
  StaticJsonDocument<384> doc;
  doc["online"]     = s.valid;
  doc["power"]      = s.power;
  doc["mode"]       = (int)s.mode;
  doc["fan"]        = (int)s.fan;
  doc["setpoint_f"] = s.setpoint_f;
  doc["indoor_c"]   = s.indoor_temp_c;
  doc["eco"]        = s.eco;
  doc["turbo"]      = s.turbo;
  doc["mute"]       = s.mute;
  doc["vswing_pos"] = s.vswing_pos;
  String body; serializeJson(doc, body);
  ws.textAll(body);
}

}
