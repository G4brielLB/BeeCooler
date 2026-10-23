// BeeCooler - firmware da central (ESP32-WROOM + DX-LR32), Fase 1.
//
// Segue a Arquitetura v1.2 (secao 7) e a Integracao em Nuvem v1.1 (secao 4):
//   task_radio  (core 1, prioridade alta): LoRa -> valida -> deduplica ->
//               persiste no LittleFS -> so entao ACK.
//   task_uplink (core 0, prioridade baixa): Wi-Fi, NTP, TLS, POST, heartbeat.
// O ACK LoRa nunca espera a nuvem. Cada registro aceito tambem sai na serial
// como uma linha `BEEREC {json}` para o coletor de bancada (D55).

#include <Arduino.h>

#include "CloudConfig.h"
#include "GatewayStore.h"
#include "RadioTask.h"
#include "UplinkTask.h"
#include "config.h"

namespace {

String gLine;

void printHelp() {
  Serial.println(
      "Comandos:\n"
      "  show                         estado e configuracao (sem o token)\n"
      "  set wifi <ssid> <senha>      (SSID sem espacos)\n"
      "  set url <https://.../v1>     base da API\n"
      "  set gw <id>                  ex.: GW01\n"
      "  set token <token>            Bearer deste gateway\n"
      "  queue                        itens pendentes / em quarentena\n"
      "  reboot"
#ifdef BEE_GATEWAY_FAULT_INJECTION
      "\n  fault corrupt <n> | fault dropack <n> | fault reboot <0|1> | "
      "fault drop200 <n>"
#endif
  );
}

void show() {
  const CloudConfig c = CloudConfigStore::load();
  const RadioTask::Stats r = RadioTask::stats();
  const Uplink::Stats u = Uplink::stats();
  Serial.printf("fw=%04X heap=%lu fs_free=%lu\n", kFirmwareVersion,
                static_cast<unsigned long>(ESP.getFreeHeap()),
                static_cast<unsigned long>(GatewayStore::freeBytes()));
  Serial.printf("cfg: ssid=%s url=%s gw=%s token=%s\n",
                c.wifi_ssid.length() ? c.wifi_ssid.c_str() : "(vazio)",
                c.api_base_url.length() ? c.api_base_url.c_str() : "(vazio)",
                c.gateway_id.length() ? c.gateway_id.c_str() : "(vazio)",
                c.token.length() ? "(definido)" : "(vazio)");
  Serial.printf("radio: frames=%lu stored=%lu status=%lu dup=%lu gaps=%lu crc=%lu "
                "acks=%lu hello=%lu persist_fail=%lu\n",
                static_cast<unsigned long>(r.frames_ok),
                static_cast<unsigned long>(r.records_stored),
                static_cast<unsigned long>(r.status_stored),
                static_cast<unsigned long>(r.duplicate_records),
                static_cast<unsigned long>(r.gap_frames),
                static_cast<unsigned long>(r.crc_failures),
                static_cast<unsigned long>(r.acks_sent),
                static_cast<unsigned long>(r.hello_seen),
                static_cast<unsigned long>(r.persist_failures));
  Serial.printf("uplink: wifi=%d ntp=%d suspended=%d ok=%lu fail=%lu http=%d "
                "remote_stored=%lu dup=%lu conflicts=%lu quarantined=%lu "
                "heap_tls=%lu\n",
                u.wifi_connected, u.ntp_valid, u.suspended,
                static_cast<unsigned long>(u.uploads_ok),
                static_cast<unsigned long>(u.uploads_failed), u.last_http_status,
                static_cast<unsigned long>(u.records_stored_remote),
                static_cast<unsigned long>(u.duplicates_remote),
                static_cast<unsigned long>(u.conflicts_remote),
                static_cast<unsigned long>(u.quarantined_batches),
                static_cast<unsigned long>(u.last_free_heap_during_tls));
}

void setConfig(const char* key, const String& value, const char* label) {
  if (value.length() == 0) {
    Serial.printf("%s: valor vazio\n", label);
    return;
  }
  const bool ok = CloudConfigStore::save(key, value);
  Serial.printf("%s %s\n", label, ok ? "gravado na NVS" : "FALHOU");
  Uplink::reloadConfig();
}

void handleLine(String line) {
  line.trim();
  if (line.length() == 0) return;
  if (line == "help") {
    printHelp();
  } else if (line == "show") {
    show();
  } else if (line == "queue") {
    Serial.printf("pendentes=%lu quarentena=%lu\n",
                  static_cast<unsigned long>(GatewayStore::pendingCount()),
                  static_cast<unsigned long>(GatewayStore::quarantineCount()));
  } else if (line == "reboot") {
    ESP.restart();
  } else if (line.startsWith("set wifi ")) {
    String rest = line.substring(9);
    const int space = rest.indexOf(' ');
    if (space < 0) {
      Serial.println("uso: set wifi <ssid> <senha>");
    } else {
      CloudConfigStore::save("ssid", rest.substring(0, space));
      CloudConfigStore::save("pass", rest.substring(space + 1));
      Serial.println("wifi gravado na NVS");
      Uplink::reloadConfig();
    }
  } else if (line.startsWith("set url ")) {
    setConfig("url", line.substring(8), "url");
  } else if (line.startsWith("set gw ")) {
    setConfig("gw", line.substring(7), "gateway_id");
  } else if (line.startsWith("set token ")) {
    setConfig("token", line.substring(10), "token");  // never echoed back
#ifdef BEE_GATEWAY_FAULT_INJECTION
  } else if (line.startsWith("fault corrupt ")) {
    RadioTask::faultCorruptEvery(line.substring(14).toInt());
  } else if (line.startsWith("fault dropack ")) {
    RadioTask::faultDropAcks(line.substring(14).toInt());
  } else if (line.startsWith("fault reboot ")) {
    RadioTask::faultRebootAfterReceive(line.substring(13).toInt() != 0);
  } else if (line.startsWith("fault drop200 ")) {
    Uplink::faultDropHttp200(line.substring(14).toInt());
#endif
  } else {
    printHelp();
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("[BeeCooler gateway] fw %04X\n", kFirmwareVersion);

  if (!GatewayStore::begin()) {
    Serial.println("LittleFS FALHOU: a central nao pode persistir; parando");
    for (;;) delay(1000);
  }
  Serial.printf("fila: %lu itens pendentes\n",
                static_cast<unsigned long>(GatewayStore::pendingCount()));

  RadioTask::start();
  Uplink::start();
  printHelp();
}

void loop() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      handleLine(gLine);
      gLine = "";
    } else if (gLine.length() < 200) {
      gLine += c;
    }
  }
  delay(10);
}
