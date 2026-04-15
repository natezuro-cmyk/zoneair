#include "http_server.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Update.h>

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
