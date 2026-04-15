#include "mdns.h"
#include <ESPmDNS.h>
#include <Arduino.h>

namespace zoneair {

void startMdns(const char* slug) {
  String host = String("zoneair-") + slug;
  if (!MDNS.begin(host.c_str())) { Serial.println("[mdns] begin failed"); return; }
  MDNS.addService("zoneair", "tcp", 80);
  MDNS.addService("http", "tcp", 80);
  Serial.printf("[mdns] %s.local\n", host.c_str());
}

}
