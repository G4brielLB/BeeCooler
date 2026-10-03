#include "UplinkTask.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_system.h>
#include <time.h>

#include <BeeCoolerLogic.h>
#include <BeeCoolerRecord.h>

#include "AmazonRootCA.h"
#include "CloudConfig.h"
#include "GatewayStore.h"
#include "RadioTask.h"
#include "config.h"

namespace Uplink {

namespace {

Stats gStats = {};
CloudConfig gConfig;
volatile bool gNtpValid = false;
volatile bool gReloadConfig = false;
uint32_t gUploadId = 0;
uint32_t gBatchItems = kMaxBatchItems;
uint8_t gBackoffStep = 0;
uint32_t gNextAttemptMs = 0;
uint32_t gLastHeartbeatMs = 0;
uint32_t gLastWifiAttemptMs = 0;
uint32_t gLastNtpMs = 0;
bool gNtpStarted = false;

#ifdef BEE_GATEWAY_FAULT_INJECTION
volatile uint32_t gDropHttp200 = 0;
#endif

uint32_t buildEpoch() {
  static uint32_t cached = 0;
  if (cached == 0) BeeCoolerLogic::parseBuildEpoch(__DATE__, __TIME__, cached);
  return cached;
}

void updateNtpFlag() {
  const time_t now = time(nullptr);
  gNtpValid = now >= static_cast<time_t>(buildEpoch());
  gStats.ntp_valid = gNtpValid;
}

void ensureWifi() {
  gStats.wifi_connected = WiFi.status() == WL_CONNECTED;
  if (gStats.wifi_connected) return;
  if (gConfig.wifi_ssid.length() == 0) return;
  if (millis() - gLastWifiAttemptMs < kWifiRetryMs && gLastWifiAttemptMs != 0) return;
  gLastWifiAttemptMs = millis();
  WiFi.mode(WIFI_STA);
  WiFi.begin(gConfig.wifi_ssid.c_str(), gConfig.wifi_password.c_str());
}

void ensureNtp() {
  if (!gStats.wifi_connected) return;
  if (!gNtpStarted || (gNtpValid && millis() - gLastNtpMs > kNtpResyncMs)) {
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    gNtpStarted = true;
    gLastNtpMs = millis();
  }
  updateNtpFlag();
}

void scheduleRetry() {
  const uint8_t steps = sizeof(kBackoffStepsMs) / sizeof(kBackoffStepsMs[0]);
  const uint32_t delay_ms =
      gBackoffStep < steps ? kBackoffStepsMs[gBackoffStep] : kBackoffSteadyMs;
  if (gBackoffStep < 255) ++gBackoffStep;
  gNextAttemptMs = millis() + delay_ms;
}

// POST with TLS validated against the Amazon root. Returns the HTTP status
// (negative = transport error) and fills `response`.
int post(const char* path, const String& body, String& response) {
  WiFiClientSecure client;
  client.setCACert(kAmazonRootCa1);  // never setInsecure()
  client.setTimeout(kHttpTimeoutMs / 1000);
  HTTPClient http;
  http.setTimeout(kHttpTimeoutMs);
  const String url = gConfig.api_base_url + path;
  if (!http.begin(client, url)) return -1;
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + gConfig.token);
  const int status = http.POST(body);
  gStats.last_free_heap_during_tls = ESP.getFreeHeap();
  if (status > 0) response = http.getString();
  http.end();
#ifdef BEE_GATEWAY_FAULT_INJECTION
  if (status == 200 && gDropHttp200 > 0) {
    --gDropHttp200;
    Serial.println("[fault] 200 descartado");
    return -99;
  }
#endif
  return status;
}

// ---- Montagem do corpo (Nuvem 5.2) ------------------------------------------

struct Run {
  uint32_t first_seq;
  std::vector<uint8_t> bytes;  // contiguous 24-byte records
};

void flushRun(Run& run, JsonArray runs) {
  if (run.bytes.empty()) return;
  const size_t cap = BeeCoolerLogic::base64EncodedSize(run.bytes.size()) + 1;
  std::vector<char> text(cap);
  BeeCoolerLogic::base64Encode(run.bytes.data(), run.bytes.size(), text.data(), cap);
  JsonObject o = runs.add<JsonObject>();
  o["first_seq"] = run.first_seq;
  o["rec_ver"] = 1;
  o["data"] = text.data();
  run.bytes.clear();
}

bool buildTelemetry(const QueueItem* items, uint32_t n, String& body) {
  JsonDocument doc;
  doc["v"] = 1;
  doc["gateway_id"] = gConfig.gateway_id;
  doc["upload_id"] = gUploadId;
  doc["gw_time"] = epochOrZero();
  JsonArray sessions = doc["sessions"].to<JsonArray>();

  bool seen[256] = {};
  for (uint32_t i = 0; i < n; ++i) {
    const uint8_t node = items[i].node_id;
    if (seen[node]) continue;
    seen[node] = true;

    JsonObject s = sessions.add<JsonObject>();
    s["node_id"] = node;
    JsonArray attempts = s["attempts"].to<JsonArray>();
    JsonArray runs = s["runs"].to<JsonArray>();

    Run run;
    run.first_seq = 0;
    bool have_status = false;
    uint32_t last_seq = 0;
    for (uint32_t j = i; j < n; ++j) {
      const QueueItem& it = items[j];
      if (it.node_id != node) continue;
      s["boot_id"] = it.boot_id;
      s["rx_time"] = it.rx_time;
      s["rssi"] = it.rssi == 0 ? 0 : -static_cast<int>(it.rssi);
      s["snr"] = it.snr;
      if (it.kind == static_cast<uint8_t>(QueueKind::kStatus)) {
        char text[BeeCoolerLogic::base64EncodedSize(36) + 1];
        BeeCoolerLogic::base64Encode(it.data, 36, text, sizeof(text));
        s["status"] = text;  // the latest STATUS of the batch wins
        have_status = true;
      } else {
        attempts.add(it.attempt);
        const bool contiguous = !run.bytes.empty() && it.seq == last_seq + 1;
        if (!contiguous) {
          flushRun(run, runs);
          run.first_seq = it.seq;
        }
        run.bytes.insert(run.bytes.end(), it.data,
                         it.data + BeeCoolerRecord::kRecordSize);
        last_seq = it.seq;
      }
    }
    flushRun(run, runs);
    if (!have_status) s["status"] = nullptr;
  }
  body = "";
  serializeJson(doc, body);
  return body.length() <= kMaxBodyBytes;
}

// ---- Resposta e tratamento de status (Nuvem 5.3) ----------------------------

void uploadPending() {
  static QueueItem items[kMaxBatchItems];  // 6.6 KB, off the task stack
  const uint32_t n = GatewayStore::peek(items, min(gBatchItems, kMaxBatchItems));
  if (n == 0) return;

  ++gUploadId;
  String body;
  if (!buildTelemetry(items, n, body)) {
    gBatchItems = max<uint32_t>(1, n / 2);  // body too large: split the batch
    return;
  }

  String response;
  const int status = post("/telemetry", body, response);
  gStats.last_http_status = status;

  if (status == 200) {
    JsonDocument r;
    if (!deserializeJson(r, response)) {
      gStats.records_stored_remote += r["stored"] | 0;
      gStats.duplicates_remote += r["duplicates"] | 0;
      gStats.conflicts_remote += r["conflicts"] | 0;
      if ((r["conflicts"] | 0) > 0) {
        Serial.printf("[uplink] ATENCAO: %d conflito(s) de seq no servidor\n",
                      static_cast<int>(r["conflicts"] | 0));
      }
      // r["commands"] is Phase 2 (UC-15, D64).
    }
    GatewayStore::consume(n);
    ++gStats.uploads_ok;
    gBackoffStep = 0;
    gBatchItems = kMaxBatchItems;
    Serial.printf("[uplink] upload %lu ok: %lu itens\n",
                  static_cast<unsigned long>(gUploadId), static_cast<unsigned long>(n));
  } else if (status == 400) {
    GatewayStore::quarantine(n);  // a malformed item must not block the queue
    ++gStats.quarantined_batches;
    ++gStats.uploads_failed;
    Serial.printf("[uplink] 400: %lu itens em quarentena\n", static_cast<unsigned long>(n));
  } else if (status == 401) {
    gStats.suspended = true;  // wait for a new token
    ++gStats.uploads_failed;
    Serial.println("[uplink] 401: token recusado, uplink suspenso");
  } else if (status == 413) {
    gBatchItems = max<uint32_t>(1, n / 2);
    ++gStats.uploads_failed;
  } else {  // 429, 5xx, transport error: keep the queue, back off
    ++gStats.uploads_failed;
    scheduleRetry();
    Serial.printf("[uplink] falha (%d); nova tentativa com backoff\n", status);
  }
}

void sendHeartbeat() {
  JsonDocument doc;
  doc["gateway_id"] = gConfig.gateway_id;
  doc["uptime_s"] = millis() / 1000U;
  doc["heap_free"] = ESP.getFreeHeap();
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["queue"] = GatewayStore::pendingCount();
  doc["quarantine"] = GatewayStore::quarantineCount();
  doc["fw"] = kFirmwareVersion;
  doc["ntp_valid"] = gNtpValid;
  doc["uplink_suspended"] = gStats.suspended;
  String body;
  serializeJson(doc, body);
  String response;
  const int status = post("/gateway/heartbeat", body, response);
  Serial.printf("[uplink] heartbeat -> %d\n", status);
  if (status == 401) gStats.suspended = true;
}

void uplinkLoop(void*) {
  gConfig = CloudConfigStore::load();
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  for (;;) {
    if (gReloadConfig) {
      gReloadConfig = false;
      gConfig = CloudConfigStore::load();
      gStats.suspended = false;  // a new token / URL deserves a new attempt
      gBackoffStep = 0;
      gNextAttemptMs = 0;
      gLastWifiAttemptMs = 0;
      WiFi.disconnect();
    }
    ensureWifi();
    ensureNtp();

    const bool ready = gStats.wifi_connected && gNtpValid && gConfig.complete() &&
                       !gStats.suspended;  // no NTP -> no TLS (4.4)
    if (ready) {
      const uint32_t now = millis();
      // Let the LoRa session finish first (~5 s after the last frame).
      const bool radio_quiet = now - RadioTask::lastActivityMs() > kUplinkSettleMs;
      if (radio_quiet && now >= gNextAttemptMs &&
          GatewayStore::pendingCount() > 0) {
        uploadPending();
      }
      if (gLastHeartbeatMs == 0 || now - gLastHeartbeatMs > kHeartbeatPeriodMs) {
        gLastHeartbeatMs = now == 0 ? 1 : now;
        sendHeartbeat();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

}  // namespace

uint32_t epochOrZero() {
  if (!gNtpValid) return 0;
  return static_cast<uint32_t>(time(nullptr));
}

Stats stats() { return gStats; }

void reloadConfig() { gReloadConfig = true; }

void start() {
  xTaskCreatePinnedToCore(uplinkLoop, "task_uplink", kUplinkTaskStack, nullptr,
                          kUplinkTaskPriority, nullptr, kUplinkTaskCore);
}

#ifdef BEE_GATEWAY_FAULT_INJECTION
void faultDropHttp200(uint32_t count) { gDropHttp200 = count; }
#endif

}  // namespace Uplink
