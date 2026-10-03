#pragma once

#include <Arduino.h>

// Credenciais e destino da nuvem, guardados na NVS (nunca no repositorio):
// gravados pelo console serial (`set wifi`, `set url`, `set gw`, `set token`).
struct CloudConfig {
  String wifi_ssid;
  String wifi_password;
  String api_base_url;  // https://<id>.execute-api.<regiao>.amazonaws.com/v1
  String gateway_id;    // ex.: GW01
  String token;         // Bearer do gateway (D62)

  bool complete() const {
    return wifi_ssid.length() > 0 && api_base_url.length() > 0 &&
           gateway_id.length() > 0 && token.length() > 0;
  }
};

namespace CloudConfigStore {
CloudConfig load();
bool save(const char* key, const String& value);
}  // namespace CloudConfigStore
