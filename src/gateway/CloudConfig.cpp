#include "CloudConfig.h"

#include <Preferences.h>

namespace CloudConfigStore {

CloudConfig load() {
  CloudConfig c;
  Preferences p;
  if (p.begin("beegw", true)) {
    c.wifi_ssid = p.getString("ssid", "");
    c.wifi_password = p.getString("pass", "");
    c.api_base_url = p.getString("url", "");
    c.gateway_id = p.getString("gw", "");
    c.token = p.getString("token", "");
    p.end();
  }
  while (c.api_base_url.endsWith("/")) c.api_base_url.remove(c.api_base_url.length() - 1);
  return c;
}

bool save(const char* key, const String& value) {
  Preferences p;
  if (!p.begin("beegw", false)) return false;
  const bool ok = p.putString(key, value) > 0 || value.length() == 0;
  p.end();
  return ok;
}

}  // namespace CloudConfigStore
