#pragma once
#include <Arduino.h>

namespace zoneair {

struct ProvisionedConfig {
  String ssid;
  String pass;
  String slug;
  bool valid;
};

class NvsStore {
 public:
  ProvisionedConfig load();
  void save(const String& ssid, const String& pass, const String& slug);
  void clear();
};

}
