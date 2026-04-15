#include "http_server.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

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

  server.on("/state", HTTP_GET, [state](AsyncWebServerRequest* req){
    StaticJsonDocument<256> doc;
    doc["online"]      = state->valid;
    doc["power"]       = state->power;
    doc["mode"]        = (int)state->mode;
    doc["fan"]         = (int)state->fan;
    doc["setpoint_c"]  = state->setpoint_c;
    doc["indoor_c"]    = state->indoor_temp_c;
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
      StaticJsonDocument<256> doc;
      if (deserializeJson(doc, data, len)) {
        auto* r = req->beginResponse(400, "application/json", "{\"error\":\"bad json\"}");
        addCors(r); req->send(r); return;
      }
      AcState desired = *state;
      if (doc.containsKey("power"))      desired.power      = doc["power"];
      if (doc.containsKey("mode"))       desired.mode       = (Mode)(int)doc["mode"];
      if (doc.containsKey("fan"))        desired.fan        = (FanSpeed)(int)doc["fan"];
      if (doc.containsKey("setpoint_c")) desired.setpoint_c = doc["setpoint_c"];
      on_set(desired);
      auto* r = req->beginResponse(200, "application/json", "{\"ok\":true}");
      addCors(r); req->send(r);
    });

  server.begin();
}

}  // namespace zoneair
