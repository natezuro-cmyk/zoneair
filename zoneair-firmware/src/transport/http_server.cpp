#include "http_server.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <Preferences.h>
#include <WiFi.h>

namespace zoneair {

AsyncWebServer& zoneair_http_server() {
  static AsyncWebServer s(80);
  return s;
}

static void addCors(AsyncWebServerResponse* r) {
  r->addHeader("Access-Control-Allow-Origin", "*");
  r->addHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  r->addHeader("Access-Control-Allow-Headers", "Content-Type");
}

void HttpServer::begin(const AcState* state, SetCommandHandler on_set) {
  auto& server = zoneair_http_server();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req){
    req->send(200, "text/html",
      "<!doctype html><html><head><meta name=viewport content='width=device-width'>"
      "<style>body{background:#0a0e14;color:#e6edf3;font-family:system-ui;padding:40px 20px;text-align:center}"
      "h1{font-weight:500;font-size:22px}p{color:#7d8896;font-size:14px;margin:12px 0}"
      "a{color:#3ea6ff}</style></head><body>"
      "<h1>Z1 Air</h1><p>This unit is online.</p>"
      "<p><a href='/state'>View state (JSON)</a></p>"
      "</body></html>");
  });

  server.on("/state", HTTP_GET, [state](AsyncWebServerRequest* req){
    StaticJsonDocument<768> doc;
    doc["online"]      = state->valid;
    doc["power"]       = state->power;
    doc["mode"]        = (int)state->mode;
    doc["fan"]         = (int)state->fan;
    doc["setpoint_c"]  = state->setpoint_c;
    doc["indoor_c"]    = state->indoor_temp_c;
    doc["eco"]         = state->eco;
    doc["turbo"]       = state->turbo;
    doc["mute"]        = state->mute;
    doc["vswing_pos"]  = state->vswing_pos;
    doc["display"]     = state->display;
    doc["beep"]        = state->beep;
    // Extended diagnostics
    doc["indoor_coil_c"]      = state->indoor_coil_c;
    doc["outdoor_temp_c"]     = state->outdoor_temp_c;
    doc["condenser_coil_c"]   = state->condenser_coil_c;
    doc["discharge_temp_c"]   = state->discharge_temp_c;
    doc["compressor_hz"]      = state->compressor_hz;
    doc["outdoor_fan_speed"]  = state->outdoor_fan_speed;
    doc["indoor_fan_speed"]   = state->indoor_fan_speed;
    doc["compressor_running"] = state->compressor_running;
    doc["four_way_valve"]     = state->four_way_valve;
    doc["antifreeze"]         = state->antifreeze;
    doc["filter_alert"]       = state->filter_alert;
    doc["supply_voltage_raw"] = state->supply_voltage_raw;
    doc["current_draw_raw"]   = state->current_draw_raw;
    doc["error_code1"]        = state->error_code1;
    doc["error_code2"]        = state->error_code2;
    String body; serializeJson(doc, body);
    auto* r = req->beginResponse(200, "application/json", body);
    addCors(r); req->send(r);
  });

  server.on("/command", HTTP_OPTIONS, [](AsyncWebServerRequest* req){
    auto* r = req->beginResponse(204);
    addCors(r); req->send(r);
  });

  server.on("/command", HTTP_POST,
    [](AsyncWebServerRequest* req){},
    nullptr,
    [state, on_set](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t){
      StaticJsonDocument<384> doc;
      if (deserializeJson(doc, data, len)) {
        auto* r = req->beginResponse(400, "application/json", "{\"error\":\"bad json\"}");
        addCors(r); req->send(r); return;
      }
      AcState desired = *state;
      if (doc.containsKey("power"))      desired.power      = doc["power"];
      if (doc.containsKey("mode"))       desired.mode       = (Mode)(int)doc["mode"];
      if (doc.containsKey("fan"))        desired.fan        = (FanSpeed)(int)doc["fan"];
      if (doc.containsKey("setpoint_c")) { desired.setpoint_c = doc["setpoint_c"]; desired.use_fahrenheit = false; }
      if (doc.containsKey("setpoint_f")) { desired.setpoint_f = (int)doc["setpoint_f"]; desired.use_fahrenheit = true; }
      if (doc.containsKey("eco"))        desired.eco        = doc["eco"];
      if (doc.containsKey("turbo"))      desired.turbo      = doc["turbo"];
      if (doc.containsKey("mute"))       desired.mute       = doc["mute"];
      if (doc.containsKey("vswing_pos")) desired.vswing_pos = (uint8_t)(int)doc["vswing_pos"];
      if (doc.containsKey("display"))    desired.display    = doc["display"];
      if (doc.containsKey("beep"))       desired.beep       = doc["beep"];
      // timer fields removed
      on_set(desired);
      auto* r = req->beginResponse(200, "application/json", "{\"ok\":true}");
      addCors(r); req->send(r);
    });

  // Factory reset: wipe BOTH our NVS namespace AND the ESP-IDF WiFi cache, reboot.
  server.on("/factory_reset", HTTP_POST, [](AsyncWebServerRequest* req){
    auto* r = req->beginResponse(200, "application/json", "{\"ok\":true}");
    addCors(r); req->send(r);
    delay(300);
    Preferences p; p.begin("zoneair", false); p.clear(); p.end();
    WiFi.disconnect(true, true);   // also wipes IDF saved creds
    delay(200);
    ESP.restart();
  });

  // OTA: raw binary in POST body. Upload with:
  //   curl --data-binary @firmware.bin -H 'Content-Type: application/octet-stream' \
  //        http://<host>/update
  server.on("/update", HTTP_POST,
    [](AsyncWebServerRequest* req){
      bool ok = !Update.hasError() && Update.isFinished();
      auto* r = req->beginResponse(ok ? 200 : 500, "text/plain", ok ? "OK\n" : "FAIL\n");
      r->addHeader("Connection", "close");
      addCors(r);
      req->send(r);
      if (ok) { delay(200); ESP.restart(); }
    },
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
      if (index == 0) {
        Serial.printf("[ota] start total=%u\n", (unsigned)total);
        if (!Update.begin(total > 0 ? total : UPDATE_SIZE_UNKNOWN)) {
          Update.printError(Serial);
        }
      }
      if (Update.write(data, len) != len) Update.printError(Serial);
      if (index + len == total) {
        if (Update.end(true)) Serial.printf("[ota] done %u bytes\n", (unsigned)(index + len));
        else Update.printError(Serial);
      }
    });

  server.begin();
}

}  // namespace zoneair
