#include "nvs_store.h"
#include <Preferences.h>

namespace zoneair {

static const char* NS = "zoneair";

ProvisionedConfig NvsStore::load() {
  Preferences p; p.begin(NS, true);
  ProvisionedConfig c;
  c.ssid = p.getString("ssid", "");
  c.pass = p.getString("pass", "");
  c.slug = p.getString("slug", "");
  c.valid = c.ssid.length() > 0;
  p.end();
  return c;
}

void NvsStore::save(const String& ssid, const String& pass, const String& slug) {
  Preferences p; p.begin(NS, false);
  p.putString("ssid", ssid);
  p.putString("pass", pass);
  p.putString("slug", slug);
  p.end();
}

void NvsStore::clear() {
  Preferences p; p.begin(NS, false);
  p.clear();
  p.end();
}

}
