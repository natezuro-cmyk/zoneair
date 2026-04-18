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
  StaticJsonDocument<768> doc;
  doc["online"]     = s.valid;
  doc["power"]      = s.power;
  doc["mode"]       = (int)s.mode;
  doc["fan"]        = (int)s.fan;
  doc["setpoint_c"] = s.setpoint_c;
  doc["indoor_c"]   = s.indoor_temp_c;
  doc["eco"]        = s.eco;
  doc["turbo"]      = s.turbo;
  doc["mute"]       = s.mute;
  doc["vswing_pos"] = s.vswing_pos;
  doc["display"]    = s.display;
  doc["beep"]       = s.beep;
  // Extended diagnostics
  doc["indoor_coil_c"]      = s.indoor_coil_c;
  doc["outdoor_temp_c"]     = s.outdoor_temp_c;
  doc["condenser_coil_c"]   = s.condenser_coil_c;
  doc["discharge_temp_c"]   = s.discharge_temp_c;
  doc["compressor_hz"]      = s.compressor_hz;
  doc["outdoor_fan_speed"]  = s.outdoor_fan_speed;
  doc["indoor_fan_speed"]   = s.indoor_fan_speed;
  doc["compressor_running"] = s.compressor_running;
  doc["four_way_valve"]     = s.four_way_valve;
  doc["antifreeze"]         = s.antifreeze;
  doc["filter_alert"]       = s.filter_alert;
  doc["supply_voltage_raw"] = s.supply_voltage_raw;
  doc["current_draw_raw"]   = s.current_draw_raw;
  doc["error_code1"]        = s.error_code1;
  doc["error_code2"]        = s.error_code2;
  String body; serializeJson(doc, body);
  ws.textAll(body);
}

}
