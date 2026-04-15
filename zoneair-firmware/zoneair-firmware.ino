#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("[zoneair] boot");
}

void loop() {
  delay(1000);
  Serial.println("[zoneair] alive");
}
