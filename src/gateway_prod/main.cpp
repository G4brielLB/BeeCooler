// BeeCooler - firmware de PRODUCAO da central (ESP32-WROOM + DX-LR32), arquivo
// unico, no formato do comunicacao_wroom_test.
//
// Segue a Arquitetura v1.2 (secao 7) e a Integracao em Nuvem v1.1 (secao 4):
//   task_radio  (core 1, prioridade alta): LoRa -> valida -> deduplica ->
//               persiste no LittleFS -> ACK -> so entao o eco serial.
//   task_uplink (core 0, prioridade baixa): Wi-Fi, NTP, TLS, POST, heartbeat.
// O ACK LoRa nunca espera a nuvem nem a serial. Cada registro aceito tambem
// sai como `BEEREC {json}` para o coletor de bancada (D55).
//
// Correcoes do relatorio da AP2 (8.2 passo 2, Tabela 12): M0/M1 como entradas
// (SWITCH=0), UART e LEVEL lidos e provisionados por AT no boot.
//
// ---- Protecoes de pilha e de hardware (revisadas para este arquivo) --------
//   Pilha: task_radio com 12 KB (LittleFS + AT do comando `radio`, ~3 KB no
//     caminho mais fundo), task_uplink com 16 KB (TLS). O lote de upload
//     (6,6 KB) e estatico; o corpo JSON fica no heap. `show` imprime a pilha
//     livre minima de cada tarefa.
//   M0/M1: com SWITCH=0 sao saidas do modulo; a central os deixa como
//     entrada. Com SWITCH=1 lido do modulo, segura os dois em LOW (normal).
//   Gravacoes no modulo: so o que difere do alvo, no maximo 3 tentativas
//     falhas (memoria do modulo), e nenhuma apos brownout ou apos 3 reinicios
//     anormais seguidos. `radio provision` no console zera a conta.
//   Flash da central: o estado do no so e regravado quando muda o que
//     precisa ser duravel antes do ACK (acked_up_to e o ultimo STATUS);
//     duplicatas, HELLO e o ensaio `rtt` do no nao gravam nada.
//   Watchdog de tarefa (300 s) nas duas tarefas, e handshake TLS limitado a
//     30 s: um travamento reinicia a central (a fila esta no LittleFS).
//   Heap: se o maior bloco livre ficar abaixo de 32 KB (o TLS deixaria de
//     conectar) por 3 minutos seguidos, a central reinicia com o radio quieto.
//   Antena: o firmware nao detecta antena. Nunca ligue sem antena: o SX1262
//     transmite o ACK a 22 dBm.

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_idf_version.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <time.h>

#include <vector>

#include <BeeCoolerLink.h>
#include <BeeCoolerLogic.h>
#include <BeeCoolerLora.h>
#include <BeeCoolerLoraAt.h>
#include <BeeCoolerRecord.h>

using namespace BeeCoolerLink;

// =============================================================================
// Configuracao
// =============================================================================

#ifndef BEE_LORA_AUTOPROVISION
#define BEE_LORA_AUTOPROVISION 1
#endif
#ifndef BEE_LORA_LEVEL
#define BEE_LORA_LEVEL 3  // igual ao do no (src/sensor_prod/main.cpp)
#endif

namespace {

constexpr uint16_t kFirmwareVersion = 0x0110;  // 1.1: producao

// ---- Pinagem (ESP32-WROOM DevKit + DX-LR32; docs/pinos.md)
constexpr int kPinLoraRx = 16;   // ESP RX2 <- TXD do modulo
constexpr int kPinLoraTx = 17;   // ESP TX2 -> RXD do modulo
constexpr int kPinLoraM0 = 19;
constexpr int kPinLoraM1 = 21;
constexpr int kPinLoraAux = 18;
static_assert(kPinLoraRx != kPinLoraTx && kPinLoraM0 != kPinLoraM1 &&
                  kPinLoraM0 != kPinLoraAux && kPinLoraM1 != kPinLoraAux,
              "dois sinais do LoRa no mesmo GPIO");
// 34-39 sao somente entrada; 6-11 sao da flash; 16/17 so livres no WROOM-32.
static_assert(kPinLoraTx < 34 && kPinLoraM0 < 34 && kPinLoraM1 < 34 &&
                  (kPinLoraTx < 6 || kPinLoraTx > 11),
              "pino de saida invalido no ESP32");

// ---- LoRa [R 8.2 passo 2]: a UART de cada lado e local; LEVEL e canal
// precisam ser iguais aos do no. Canal, chave e potencia nunca sao alterados.
constexpr bool kAutoProvision = BEE_LORA_AUTOPROVISION != 0;
constexpr uint32_t kTargetBaud = 115200U;
constexpr uint32_t kFactoryBaud = 9600U;   // BAUD=3, confirmado na bancada
constexpr int kLevel = BEE_LORA_LEVEL;
constexpr uint8_t kBaudCodes115200[] = {7U, 8U};  // conferidos pela varredura
constexpr uint8_t kMaxProvisionFailures = 3U;
constexpr uint8_t kMaxNodes = 8;
static_assert(kLevel >= 0 && kLevel <= 7, "BEE_LORA_LEVEL 0..7");

// ---- Fila de uplink [N 4.2]
constexpr size_t kQueueItemSize = 52U;
constexpr uint32_t kMaxBatchItems = 128U;
constexpr size_t kMaxBodyBytes = 32U * 1024U;
constexpr uint32_t kQueueCompactThreshold = 512U;
constexpr uint32_t kUplinkSettleMs = 5000U;  // espera apos o ultimo quadro LoRa
constexpr uint32_t kBackoffStepsMs[] = {5000U, 15000U, 60000U};
constexpr uint32_t kBackoffSteadyMs = 300000U;

// ---- Heartbeat, relogio e rede [N 4.3, 4.4]
constexpr uint32_t kHeartbeatPeriodMs = 15U * 60U * 1000U;
constexpr uint32_t kNtpResyncMs = 6U * 60U * 60U * 1000U;
constexpr uint32_t kHttpTimeoutMs = 10000U;
constexpr uint32_t kTlsHandshakeTimeoutS = 30U;
constexpr uint32_t kWifiRetryMs = 15000U;

// ---- Tarefas [N 4.1]
constexpr uint32_t kRadioTaskStack = 12288U;
constexpr uint32_t kUplinkTaskStack = 16384U;
constexpr int kRadioTaskPriority = 5;
constexpr int kUplinkTaskPriority = 1;
constexpr int kRadioTaskCore = 1;
constexpr int kUplinkTaskCore = 0;

// ---- Protecoes
constexpr uint32_t kWatchdogS = 300U;
constexpr uint8_t kNoWritesAfterResets = 3U;
constexpr uint32_t kHealthyUptimeMs = 10U * 60U * 1000U;
constexpr uint32_t kMinHeapBlockBytes = 32U * 1024U;
constexpr uint8_t kLowHeapChecksBeforeRestart = 3U;  // uma por minuto

// =============================================================================
// Amazon Root CA 1 (copiada de src/gateway/AmazonRootCA.h)
// =============================================================================

// Amazon Root CA 1 (Amazon Trust Services), self-signed, valid until 2038-01-17.
// Source: https://www.amazontrust.com/repository/AmazonRootCA1.pem
// SHA-256 fingerprint (verified when this file was generated):
//   8E:CD:E6:88:4F:3D:87:B1:12:5B:A3:1A:C3:FC:B1:3D:70:16:DE:7F:57:CC:90:4F:E1:CB:97:C6:AE:98:19:6E
//
// The uplink validates the API Gateway endpoint against this root and never
// falls back to setInsecure() (Integracao em Nuvem v1.1, 4.4). If AWS changes
// the chain, replace it here and reflash (pending item P-ROOT).

static const char kAmazonRootCa1[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF\n"
    "ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n"
    "b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL\n"
    "MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\n"
    "b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj\n"
    "ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM\n"
    "9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw\n"
    "IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6\n"
    "VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L\n"
    "93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm\n"
    "jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n"
    "AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA\n"
    "A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI\n"
    "U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs\n"
    "N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv\n"
    "o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU\n"
    "5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy\n"
    "rqXRfboQnoZsG4q5WTP468SQvvG5\n"
    "-----END CERTIFICATE-----\n";

// =============================================================================
// Rele de reinicio e watchdog
// =============================================================================

struct BootGuard {
  uint32_t magic;
  uint8_t abnormal_resets;
  uint8_t last_reason;
  uint16_t reserved;
};
constexpr uint32_t kBootGuardMagic = 0xB007AB1EU;
RTC_NOINIT_ATTR BootGuard gBoot;
bool gRadioWritesAllowed = true;

bool abnormalReset(esp_reset_reason_t r) {
  return r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT ||
         r == ESP_RST_WDT || r == ESP_RST_BROWNOUT;
}

void watchdogConfigure() {
#if ESP_IDF_VERSION_MAJOR >= 5
  const esp_task_wdt_config_t cfg = {kWatchdogS * 1000U, 0U, true};
  if (esp_task_wdt_reconfigure(&cfg) != ESP_OK) esp_task_wdt_init(&cfg);
#else
  esp_task_wdt_init(kWatchdogS, true);  // IDF 4: reconfigura se ja ativo
#endif
}

inline void feed() { esp_task_wdt_reset(); }

// =============================================================================
// Configuracao da nuvem na NVS (nunca no repositorio)
// =============================================================================

struct CloudConfig {
  String wifi_ssid;
  String wifi_password;
  String api_base_url;  // https://<id>.execute-api.<regiao>.amazonaws.com/v1
  String gateway_id;    // ex.: GW01
  String token;         // Bearer da central (D62)
  bool complete() const {
    return wifi_ssid.length() > 0 && api_base_url.length() > 0 && gateway_id.length() > 0 &&
           token.length() > 0;
  }
};

CloudConfig loadCloudConfig() {
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

bool saveCloudValue(const char* key, const String& value) {
  Preferences p;
  if (!p.begin("beegw", false)) return false;
  const bool ok = p.putString(key, value) > 0 || value.length() == 0;
  p.end();
  return ok;
}

// =============================================================================
// Persistencia no LittleFS (Arquitetura 7.1/7.2; Nuvem 4.2)
//   /queue.bin  itens de tamanho fixo esperando o uplink
//   /qhead.bin  quantos itens da fila ja foram consumidos
//   /quar.bin   itens de lotes que a API recusou com 400
//   /n<id>.bin  estado de cada no (acked_up_to, ultimo STATUS)
// As duas tarefas compartilham; toda chamada publica pega o mutex.
// =============================================================================

enum class QueueKind : uint8_t { kRecord = 1, kStatus = 2 };

struct __attribute__((packed)) QueueItem {
  uint8_t kind;
  uint8_t node_id;
  uint8_t boot_id;
  uint8_t attempt;
  uint8_t rssi;      // -dBm na central (0 = desconhecido)
  int8_t snr;
  uint16_t reserved;
  uint32_t seq;      // registro: seu seq; STATUS: seq do registro embutido
  uint32_t rx_time;  // epoch NTP da central na recepcao (0 = NTP invalido)
  uint8_t data[36];  // registro: 24 B; STATUS: 36 B
};
static_assert(sizeof(QueueItem) == kQueueItemSize, "tamanho do item da fila");

struct NodeRecordState {
  uint32_t magic;
  uint8_t node_id;
  uint8_t last_boot_id;
  uint16_t last_status_frame_seq;
  uint32_t acked_up_to;           // maior seq contiguo persistido aqui
  uint32_t last_rx_epoch;
  uint32_t last_status_boot_key;
  uint8_t last_rssi;
  int8_t last_snr;
  uint16_t reserved;
  uint32_t attempt_histogram[5];  // [1..4] quadros recebidos naquela tentativa
};
constexpr uint32_t kNodeStateMagic = 0xBEE06A11U;  // mesmo formato de src/gateway

namespace store {

constexpr const char* kQueuePath = "/queue.bin";
constexpr const char* kHeadPath = "/qhead.bin";
constexpr const char* kQuarantinePath = "/quar.bin";

SemaphoreHandle_t gMutex = nullptr;
NodeRecordState gNodes[kMaxNodes];
bool gNodeLoaded[kMaxNodes] = {};

struct Lock {
  Lock() { xSemaphoreTake(gMutex, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(gMutex); }
};

uint32_t readHeadLocked() {
  File f = LittleFS.open(kHeadPath, "r");
  if (!f || f.size() != sizeof(uint32_t)) return 0;
  uint32_t head = 0;
  f.read(reinterpret_cast<uint8_t*>(&head), sizeof(head));
  return head;
}

bool writeHeadLocked(uint32_t head) {
  File f = LittleFS.open(kHeadPath, "w");
  if (!f) return false;
  const bool ok = f.write(reinterpret_cast<const uint8_t*>(&head), sizeof(head)) == sizeof(head);
  f.close();
  return ok;
}

uint32_t totalLocked() {
  File f = LittleFS.open(kQueuePath, "r");
  return f ? static_cast<uint32_t>(f.size() / kQueueItemSize) : 0;
}

// Tira o prefixo consumido: apaga tudo quando esvazia; senao reescreve o
// restante quando o peso morto passa do limite.
void compactLocked(uint32_t head, uint32_t total) {
  if (head >= total) {
    LittleFS.remove(kQueuePath);
    LittleFS.remove(kHeadPath);
    return;
  }
  if (head < kQueueCompactThreshold) return;
  File src = LittleFS.open(kQueuePath, "r");
  File dst = LittleFS.open("/queue.tmp", "w");
  if (!src || !dst) return;
  src.seek(static_cast<size_t>(head) * kQueueItemSize);
  uint8_t buffer[kQueueItemSize];
  bool ok = true;
  for (uint32_t i = head; i < total && ok; ++i) {
    ok = src.read(buffer, sizeof(buffer)) == sizeof(buffer) &&
         dst.write(buffer, sizeof(buffer)) == sizeof(buffer);
  }
  src.close();
  dst.close();
  if (!ok) {
    LittleFS.remove("/queue.tmp");
    return;
  }
  // Queda no meio: head = 0 sobre o arquivo antigo so reenvia (a nuvem
  // grava sem duplicar); um head velho sobre o arquivo novo pularia itens.
  // Por isso zera o head antes, e o rename troca a fila de forma atomica.
  writeHeadLocked(0);
  LittleFS.rename("/queue.tmp", kQueuePath);
}

bool begin() {
  gMutex = xSemaphoreCreateMutex();
  if (gMutex == nullptr) return false;
  if (!LittleFS.begin(true)) return false;  // true: formata no primeiro uso
  Lock lock;
  // Uma queda no meio da escrita pode deixar um item parcial: corta a cauda.
  File f = LittleFS.open(kQueuePath, "r");
  if (f) {
    const size_t size = f.size();
    f.close();
    const size_t whole = (size / kQueueItemSize) * kQueueItemSize;
    if (whole != size) {
      File in = LittleFS.open(kQueuePath, "r");
      File out = LittleFS.open("/queue.tmp", "w");
      uint8_t buffer[kQueueItemSize];
      for (size_t off = 0; off < whole && in && out; off += kQueueItemSize) {
        if (in.read(buffer, sizeof(buffer)) != sizeof(buffer)) break;
        out.write(buffer, sizeof(buffer));
      }
      in.close();
      out.close();
      LittleFS.rename("/queue.tmp", kQueuePath);
    }
  }
  if (readHeadLocked() > totalLocked()) writeHeadLocked(0);
  return true;
}

bool appendItem(const QueueItem& item) {
  Lock lock;
  File f = LittleFS.open(kQueuePath, "a");
  if (!f) return false;
  const bool ok = f.write(reinterpret_cast<const uint8_t*>(&item), sizeof(item)) == sizeof(item);
  f.close();  // o close grava na flash: so depois disso pode haver ACK
  return ok;
}

uint32_t pendingCount() {
  Lock lock;
  const uint32_t total = totalLocked();
  const uint32_t head = readHeadLocked();
  return total > head ? total - head : 0;
}

uint32_t quarantineCount() {
  Lock lock;
  File f = LittleFS.open(kQuarantinePath, "r");
  return f ? static_cast<uint32_t>(f.size() / kQueueItemSize) : 0;
}

uint32_t peek(QueueItem* out, uint32_t max) {
  Lock lock;
  const uint32_t total = totalLocked();
  const uint32_t head = readHeadLocked();
  if (total <= head) return 0;
  File f = LittleFS.open(kQueuePath, "r");
  if (!f) return 0;
  f.seek(static_cast<size_t>(head) * kQueueItemSize);
  uint32_t n = 0;
  while (n < max && head + n < total &&
         f.read(reinterpret_cast<uint8_t*>(&out[n]), sizeof(QueueItem)) == sizeof(QueueItem)) {
    ++n;
  }
  return n;
}

bool consume(uint32_t count) {
  Lock lock;
  const uint32_t total = totalLocked();
  const uint32_t head = min(total, readHeadLocked() + count);
  if (!writeHeadLocked(head)) return false;
  compactLocked(head, total);
  return true;
}

bool quarantine(uint32_t count) {
  {
    Lock lock;
    const uint32_t total = totalLocked();
    const uint32_t head = readHeadLocked();
    File src = LittleFS.open(kQueuePath, "r");
    File dst = LittleFS.open(kQuarantinePath, "a");
    if (!src || !dst) return false;
    src.seek(static_cast<size_t>(head) * kQueueItemSize);
    QueueItem item;
    for (uint32_t i = 0; i < count && head + i < total; ++i) {
      if (src.read(reinterpret_cast<uint8_t*>(&item), sizeof(item)) != sizeof(item)) break;
      dst.write(reinterpret_cast<const uint8_t*>(&item), sizeof(item));
    }
  }
  return consume(count);
}

// Um slot por id modulo kMaxNodes; colisao = "sem espaco".
NodeRecordState* node(uint8_t node_id) {
  if (node_id == 0) return nullptr;
  Lock lock;
  const uint8_t slot = static_cast<uint8_t>(node_id % kMaxNodes);
  NodeRecordState& s = gNodes[slot];
  if (!gNodeLoaded[slot]) {
    char path[16];
    snprintf(path, sizeof(path), "/n%u.bin", node_id);
    memset(&s, 0, sizeof(s));
    File f = LittleFS.open(path, "r");
    if (f && f.size() == sizeof(s)) f.read(reinterpret_cast<uint8_t*>(&s), sizeof(s));
    if (s.magic != kNodeStateMagic) {
      memset(&s, 0, sizeof(s));
      s.magic = kNodeStateMagic;
      s.node_id = node_id;
    }
    gNodeLoaded[slot] = true;
  }
  return s.node_id == node_id ? &s : nullptr;
}

bool saveNode(const NodeRecordState& state) {
  Lock lock;
  char path[16];
  snprintf(path, sizeof(path), "/n%u.bin", state.node_id);
  File f = LittleFS.open(path, "w");
  if (!f) return false;
  const bool ok = f.write(reinterpret_cast<const uint8_t*>(&state), sizeof(state)) == sizeof(state);
  f.close();
  return ok;
}

size_t freeBytes() {
  Lock lock;
  return LittleFS.totalBytes() - LittleFS.usedBytes();
}

}  // namespace store

// =============================================================================
// task_uplink (Nuvem 4.1): Wi-Fi, NTP, TLS, POST /v1/telemetry, heartbeat
// =============================================================================

namespace uplink {

struct Stats {
  bool wifi_connected;
  bool ntp_valid;
  bool suspended;  // 401: para ate o token mudar
  uint32_t uploads_ok;
  uint32_t uploads_failed;
  uint32_t records_stored_remote;
  uint32_t duplicates_remote;
  uint32_t conflicts_remote;
  uint32_t quarantined_batches;
  int last_http_status;
  uint32_t last_free_heap_during_tls;
};

Stats gStats = {};
CloudConfig gConfig;
volatile bool gNtpValid = false;
volatile bool gReloadConfig = false;
TaskHandle_t gTask = nullptr;
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

// Epoch UTC com NTP valido; senao 0 (o no recusa).
uint32_t epochOrZero() {
  if (!gNtpValid) return 0;
  return static_cast<uint32_t>(time(nullptr));
}

void updateNtpFlag() {
  gNtpValid = time(nullptr) >= static_cast<time_t>(buildEpoch());
  gStats.ntp_valid = gNtpValid;
}

void ensureWifi() {
  gStats.wifi_connected = WiFi.status() == WL_CONNECTED;
  if (gStats.wifi_connected || gConfig.wifi_ssid.length() == 0) return;
  if (gLastWifiAttemptMs != 0 && millis() - gLastWifiAttemptMs < kWifiRetryMs) return;
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
  const uint32_t delay_ms = gBackoffStep < steps ? kBackoffStepsMs[gBackoffStep] : kBackoffSteadyMs;
  if (gBackoffStep < 255) ++gBackoffStep;
  gNextAttemptMs = millis() + delay_ms;
}

// POST com TLS validado pela raiz da Amazon (nunca setInsecure). Devolve o
// status HTTP (negativo = erro de transporte).
int post(const char* path, const String& body, String& response) {
  WiFiClientSecure client;
  client.setCACert(kAmazonRootCa1);
  client.setTimeout(kHttpTimeoutMs / 1000);
  client.setHandshakeTimeout(kTlsHandshakeTimeoutS);
  HTTPClient http;
  http.setTimeout(kHttpTimeoutMs);
  http.setConnectTimeout(kHttpTimeoutMs);
  const String url = gConfig.api_base_url + path;
  if (!http.begin(client, url)) return -1;
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + gConfig.token);
  feed();
  const int status = http.POST(body);
  feed();
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

// ---- Corpo do POST (Nuvem 5.2)

struct Run {
  uint32_t first_seq;
  std::vector<uint8_t> bytes;  // registros de 24 B contiguos
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

  static bool seen[256];  // fora da pilha
  memset(seen, 0, sizeof(seen));
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
        s["status"] = text;  // o STATUS mais recente do lote prevalece
        have_status = true;
      } else {
        attempts.add(it.attempt);
        const bool contiguous = !run.bytes.empty() && it.seq == last_seq + 1;
        if (!contiguous) {
          flushRun(run, runs);
          run.first_seq = it.seq;
        }
        run.bytes.insert(run.bytes.end(), it.data, it.data + BeeCoolerRecord::kRecordSize);
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

uint32_t radioLastActivityMs();  // definido em radio::

// ---- Resposta (Nuvem 5.3): 200 tira da fila, 400 quarentena, 401 suspende,
// 413 divide o lote, 429/5xx/transporte retentam com espera crescente.
void uploadPending() {
  static QueueItem items[kMaxBatchItems];  // 6,6 KB fora da pilha
  const uint32_t n = store::peek(items, min(gBatchItems, kMaxBatchItems));
  if (n == 0) return;
  ++gUploadId;
  String body;
  if (!buildTelemetry(items, n, body)) {
    gBatchItems = max<uint32_t>(1, n / 2);
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
    }
    store::consume(n);
    ++gStats.uploads_ok;
    gBackoffStep = 0;
    gBatchItems = kMaxBatchItems;
    Serial.printf("[uplink] upload %lu ok: %lu itens\n", static_cast<unsigned long>(gUploadId),
                  static_cast<unsigned long>(n));
  } else if (status == 400) {
    store::quarantine(n);  // um item malformado nao pode travar a fila
    ++gStats.quarantined_batches;
    ++gStats.uploads_failed;
    Serial.printf("[uplink] 400: %lu itens em quarentena\n", static_cast<unsigned long>(n));
  } else if (status == 401) {
    gStats.suspended = true;
    ++gStats.uploads_failed;
    Serial.println("[uplink] 401: token recusado, uplink suspenso");
  } else if (status == 413) {
    gBatchItems = max<uint32_t>(1, n / 2);
    ++gStats.uploads_failed;
  } else {
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
  doc["queue"] = store::pendingCount();
  doc["quarantine"] = store::quarantineCount();
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

void loopTask(void*) {
  esp_task_wdt_add(nullptr);
  gConfig = loadCloudConfig();
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  for (;;) {
    feed();
    if (gReloadConfig) {
      gReloadConfig = false;
      gConfig = loadCloudConfig();
      gStats.suspended = false;  // token ou URL novos merecem nova tentativa
      gBackoffStep = 0;
      gNextAttemptMs = 0;
      gLastWifiAttemptMs = 0;
      WiFi.disconnect();
    }
    ensureWifi();
    ensureNtp();
    const bool ready = gStats.wifi_connected && gNtpValid && gConfig.complete() && !gStats.suspended;
    if (ready) {
      const uint32_t now = millis();
      const bool radio_quiet = now - radioLastActivityMs() > kUplinkSettleMs;
      if (radio_quiet && static_cast<int32_t>(now - gNextAttemptMs) >= 0 &&
          store::pendingCount() > 0) {
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

void start() {
  xTaskCreatePinnedToCore(loopTask, "task_uplink", kUplinkTaskStack, nullptr, kUplinkTaskPriority,
                          &gTask, kUplinkTaskCore);
}

}  // namespace uplink

// =============================================================================
// task_radio (Nuvem 4.1): unica tarefa que toca a UART do LoRa
// =============================================================================

namespace radio {

struct Stats {
  uint32_t frames_ok;
  uint32_t crc_failures;
  uint32_t discarded_bytes;
  uint32_t duplicate_records;
  uint32_t gap_frames;       // DATA alem de acked_up_to + 1: nao gravado
  uint32_t records_stored;
  uint32_t status_stored;
  uint32_t hello_seen;
  uint32_t acks_sent;
  uint32_t persist_failures;
  uint32_t node_writes;      // regravacoes do estado de no no LittleFS
  uint32_t max_handle_us;    // quadro decodificado -> ACK entregue ao modulo
};

struct Info {
  bool at_ok;
  uint32_t uart_baud;
  int level;
  int channel;
  int switch_mode;
  bool pin_control;
};

BeeCoolerLora gLora;
StreamParser gParser;
Stats gStats = {};
Info gInfo = {};
TaskHandle_t gTask = nullptr;
volatile uint32_t gLastActivityMs = 0;
volatile int8_t gCheckRequest = -1;  // -1 nada, 0 conferir, 1 conferir + provisionar

#ifdef BEE_GATEWAY_FAULT_INJECTION
volatile uint32_t gCorruptEvery = 0;
volatile uint32_t gDropAcks = 0;
volatile bool gRebootAfterReceive = false;
uint32_t gReceived = 0;
#endif

BeeCoolerLoraAt::Port makePort(bool pin_control) {
  return {&Serial2,
          static_cast<int8_t>(kPinLoraRx),
          static_cast<int8_t>(kPinLoraTx),
          static_cast<int8_t>(kPinLoraM0),
          static_cast<int8_t>(kPinLoraM1),
          static_cast<int8_t>(kPinLoraAux),
          pin_control};
}

// AT (e provisionamento) no boot ou pelo console. Deixa o driver em normal.
void bringUp(bool provision, bool force) {
  uint32_t cached = 0;
  uint8_t failures = 0;
  Preferences p;
  if (p.begin("beegw", true)) {
    cached = p.getUInt("lbaud", 0U);
    failures = p.getUChar("lprov", 0U);
    p.end();
  }
  // M0/M1 como entradas: com SWITCH=0 (fabrica) sao saidas do modulo.
  BeeCoolerLoraAt::Port port = makePort(false);
  BeeCoolerLoraAt::applyPinMode(port);
  BeeCoolerLoraAt::Config cfg;
  bool ok = BeeCoolerLoraAt::detect(port, cached != 0U ? cached : kFactoryBaud, cfg, &Serial);
  feed();

  if (ok) {
    BeeCoolerLoraAt::printConfig(cfg, Serial);
    const bool may_write =
        provision && (force || (kAutoProvision && gRadioWritesAllowed &&
                                failures < kMaxProvisionFailures));
    if (provision && !may_write) {
      Serial.printf("[LoRa] provisionamento suspenso (%s); console: radio provision\n",
                    !gRadioWritesAllowed ? "reinicio anormal/brownout" : "3 falhas seguidas");
    }
    if (may_write) {
      const BeeCoolerLoraAt::Target target = {kTargetBaud, kLevel, BeeCoolerLoraAt::kUnknown,
                                              kBaudCodes115200, sizeof(kBaudCodes115200)};
      const BeeCoolerLoraAt::Outcome o = BeeCoolerLoraAt::provision(port, target, cfg, &Serial);
      feed();
      Serial.printf("[LoRa] provisionamento: %s\n", BeeCoolerLoraAt::outcomeName(o));
      const bool failed =
          o == BeeCoolerLoraAt::Outcome::kFailed || o == BeeCoolerLoraAt::Outcome::kPartial;
      const uint8_t next = failed ? static_cast<uint8_t>(min<int>(255, failures + 1)) : 0U;
      if (next != failures && p.begin("beegw", false)) {
        p.putUChar("lprov", next);
        p.end();
      }
      ok = cfg.valid;
      if (ok && o == BeeCoolerLoraAt::Outcome::kChanged) BeeCoolerLoraAt::printConfig(cfg, Serial);
    }
  }

  gInfo.at_ok = ok;
  gInfo.uart_baud = ok ? cfg.uart_baud : (cached != 0U ? cached : kFactoryBaud);
  gInfo.level = ok ? cfg.level : BeeCoolerLoraAt::kUnknown;
  gInfo.channel = ok ? cfg.channel : BeeCoolerLoraAt::kUnknown;
  gInfo.switch_mode = ok ? cfg.switch_mode : BeeCoolerLoraAt::kUnknown;
  // SWITCH=1: pinos do modulo sao entradas; soltos seria sleep: LOW (normal).
  gInfo.pin_control = ok && cfg.switch_mode == 1;
  if (ok && cfg.uart_baud != cached && p.begin("beegw", false)) {
    p.putUInt("lbaud", cfg.uart_baud);  // so quando muda (desgaste da NVS)
    p.end();
  }
  if (!ok) Serial.println("[LoRa] ATENCAO: modulo nao respondeu AT; seguindo com a UART da NVS/fabrica");
  if (ok && cfg.level != kLevel) {
    Serial.printf("[LoRa] ATENCAO: LEVEL %d diferente do alvo %d (o no precisa do mesmo)\n",
                  cfg.level, kLevel);
  }

  const BeeCoolerLora::Pins pins = {kPinLoraRx, kPinLoraTx, kPinLoraM0, kPinLoraM1, kPinLoraAux};
  gLora.begin(Serial2, pins, gInfo.uart_baud, gInfo.pin_control);
  gLora.setMode(BeeCoolerLora::Mode::kNormal);  // sempre ouvindo
  gLora.flushInput();
  gParser.reset();
  Serial.printf("[LoRa] central ouvindo: UART %lu Bd, LEVEL %d (alvo %d), M0/M1 %s\n",
                static_cast<unsigned long>(gInfo.uart_baud), gInfo.level, kLevel,
                gInfo.pin_control ? "em LOW (SWITCH=1)" : "entradas (SWITCH=0)");
}

void sendAck(const Frame& f, uint32_t acked_up_to) {
#ifdef BEE_GATEWAY_FAULT_INJECTION
  if (gDropAcks > 0) {
    --gDropAcks;
    Serial.println("[fault] ACK descartado");
    return;
  }
#endif
  Ack ack = {};
  ack.node_id = f.header.node_id;
  ack.frame_seq = f.header.frame_seq;
  ack.acked_up_to = acked_up_to;
  ack.epoch = uplink::epochOrZero();  // 0 sem NTP: o no recusa
  // [R Tabela 12] modo transparente sem DRSSI: RSSI/SNR desconhecidos (0).
  ack.rssi = 0;
  ack.snr = 0;
  ack.pending_command = 0;
  uint8_t buffer[kAckSize];
  size_t size = 0;
  if (encodeAck(ack, buffer, sizeof(buffer), size) != Result::kOk) return;
  uint32_t tx_end_us = 0;
  if (gLora.send(buffer, size, tx_end_us)) ++gStats.acks_sent;
  gLastActivityMs = millis();
}

void echoRecord(uint8_t node, uint32_t seq, uint8_t attempt, const BeeCoolerRecord::RecordV1& r) {
  Serial.printf("BEEREC {\"node\":%u,\"seq\":%lu,\"attempt\":%u,\"ts\":%lu,"
                "\"t_in\":%d,\"rh_in\":%u,\"t_out\":%d,\"rh_out\":%u,"
                "\"vbat_mv\":%u,\"vib_rms\":%u,\"vib_peak\":%u,"
                "\"bands\":[%u,%u,%u,%u],\"flags\":%u}\n",
                node, static_cast<unsigned long>(seq), attempt, static_cast<unsigned long>(r.ts),
                r.t_in, r.rh_in, r.t_out, r.rh_out, r.vbat_mv, r.vib_rms, r.vib_peak, r.band_db[0],
                r.band_db[1], r.band_db[2], r.band_db[3], r.flags);
}

QueueItem makeItem(QueueKind kind, const Frame& f, uint32_t seq) {
  QueueItem item;
  memset(&item, 0, sizeof(item));
  item.kind = static_cast<uint8_t>(kind);
  item.node_id = f.header.node_id;
  item.boot_id = f.header.boot_id;
  item.attempt = f.header.attempt;
  item.seq = seq;
  item.rx_time = uplink::epochOrZero();
  return item;
}

struct Echo {
  uint32_t seq;
  BeeCoolerRecord::RecordV1 record;
};

// 7.1: valida -> insere sem duplicar -> persiste -> atualiza acked_up_to ->
// ACK -> so entao o eco serial.
void handleFrame(const Frame& f, uint32_t decoded_us) {
  if (f.is_ack || f.header.node_id == 0 || f.header.node_id == 0xFF) return;
  NodeRecordState* node = store::node(f.header.node_id);
  if (node == nullptr) return;  // mais nos que kMaxNodes
  ++gStats.frames_ok;

#ifdef BEE_GATEWAY_FAULT_INJECTION
  if (gRebootAfterReceive) {
    Serial.println("[fault] reiniciando entre recepcao e persistencia");
    Serial.flush();
    ESP.restart();
  }
#endif

  bool persisted = true;
  const uint32_t acked_before = node->acked_up_to;
  const uint32_t status_key_before = node->last_status_boot_key;
  Echo echoes[kMaxRecordsPerFrame];
  uint8_t n_echo = 0;
  bool hello_valid = false;
  Hello hello = {};

  switch (f.header.type) {
    case FrameType::kData: {
      const uint32_t first = f.header.first_seq;
      const uint32_t last = first + f.header.n_rec - 1U;
      if (last <= node->acked_up_to) {
        gStats.duplicate_records += f.header.n_rec;  // ja persistido: so re-ACK
      } else if (first > node->acked_up_to + 1U) {
        ++gStats.gap_frames;  // o no reenvia a partir de acked_up_to + 1
      } else {
        for (uint8_t i = 0; i < f.header.n_rec && persisted; ++i) {
          const uint32_t seq = first + i;
          if (seq <= node->acked_up_to) continue;
          QueueItem item = makeItem(QueueKind::kRecord, f, seq);
          memcpy(item.data, f.payload + i * BeeCoolerRecord::kRecordSize,
                 BeeCoolerRecord::kRecordSize);
          persisted = store::appendItem(item);
          if (persisted) {
            node->acked_up_to = seq;
            ++gStats.records_stored;
            if (n_echo < kMaxRecordsPerFrame) {
              echoes[n_echo].seq = seq;
              memcpy(&echoes[n_echo].record, item.data, sizeof(echoes[n_echo].record));
              ++n_echo;
            }
          }
        }
      }
      if (f.header.attempt >= 1 && f.header.attempt <= 4) ++node->attempt_histogram[f.header.attempt];
      break;
    }
    case FrameType::kStatus: {
      const uint32_t key = (static_cast<uint32_t>(f.header.boot_id) << 16) | f.header.frame_seq;
      if (key != node->last_status_boot_key) {  // retransmissoes reusam frame_seq
        QueueItem item = makeItem(QueueKind::kStatus, f, f.header.first_seq);
        memcpy(item.data, f.payload, kStatusPayloadSize);
        persisted = store::appendItem(item);
        if (persisted) {
          node->last_status_boot_key = key;
          ++gStats.status_stored;
        }
      }
      if (f.header.attempt >= 1 && f.header.attempt <= 4) ++node->attempt_histogram[f.header.attempt];
      break;
    }
    case FrameType::kHello:
      if (decodeHello(f.payload, f.payload_length, hello) == Result::kOk) {
        ++gStats.hello_seen;
        hello_valid = true;
      }
      break;
    default:
      return;  // EVENT e da Fase 2; sem ACK
  }

  node->last_boot_id = f.header.boot_id;
  node->last_rx_epoch = uplink::epochOrZero();
  // So o que o ACK confirma precisa estar na flash antes dele; o resto
  // (historico, ultimo contato) vai junto na proxima gravacao.
  const bool durable_change =
      node->acked_up_to != acked_before || node->last_status_boot_key != status_key_before;
  if (durable_change) {
    persisted = store::saveNode(*node) && persisted;
    ++gStats.node_writes;
  }
  if (!persisted) {
    ++gStats.persist_failures;
    node->acked_up_to = acked_before;  // nunca confirma o que nao foi guardado
    node->last_status_boot_key = status_key_before;
    Serial.printf("[radio] FALHA ao persistir node=%u: sem ACK, o no retransmite\n",
                  f.header.node_id);
    return;
  }
  sendAck(f, node->acked_up_to);
  const uint32_t handle_us = micros() - decoded_us;
  if (handle_us > gStats.max_handle_us) gStats.max_handle_us = handle_us;

  for (uint8_t i = 0; i < n_echo; ++i) {
    echoRecord(f.header.node_id, echoes[i].seq, f.header.attempt, echoes[i].record);
  }
  if (hello_valid) {
    Serial.printf("[hello] node=%u fw=%04X reset=%u rtc_valid=%u last_seq=%lu boots=%u "
                  "(acked_up_to=%lu)\n",
                  f.header.node_id, hello.fw_version, hello.reset_reason, hello.rtc_valid,
                  static_cast<unsigned long>(hello.last_seq), hello.boot_count,
                  static_cast<unsigned long>(node->acked_up_to));
  }
}

void loopTask(void*) {
  esp_task_wdt_add(nullptr);
  for (;;) {
    feed();
    const int8_t request = gCheckRequest;
    if (request >= 0) {
      gCheckRequest = -1;
      bringUp(true, request == 1);  // 0: so confere (sem gravar); 1: provisiona
    }
    bool idle = true;
    while (gLora.available() > 0) {
      idle = false;
      bool got = gParser.feed(static_cast<uint8_t>(gLora.read()));
      while (got) {
        Frame f;
        if (decodeFrame(gParser.frame(), gParser.frameSize(), f) == Result::kOk) {
          const uint32_t decoded_us = micros();
#ifdef BEE_GATEWAY_FAULT_INJECTION
          if (gCorruptEvery > 0 && ++gReceived % gCorruptEvery == 0U) {
            Serial.println("[fault] quadro tratado como corrompido");
            got = gParser.poll();
            continue;
          }
#endif
          gLastActivityMs = millis();
          handleFrame(f, decoded_us);
        }
        got = gParser.poll();
      }
    }
    gStats.crc_failures = gParser.crcFailures();
    gStats.discarded_bytes = gParser.discardedBytes();
    if (idle) vTaskDelay(pdMS_TO_TICKS(2));
  }
}

void start() {
  bringUp(true, false);
  xTaskCreatePinnedToCore(loopTask, "task_radio", kRadioTaskStack, nullptr, kRadioTaskPriority,
                          &gTask, kRadioTaskCore);
}

}  // namespace radio

uint32_t uplink::radioLastActivityMs() { return radio::gLastActivityMs; }

// =============================================================================
// Saude do heap, console e setup/loop
// =============================================================================

uint8_t gLowHeapChecks = 0;
uint32_t gLastHealthMs = 0;
String gLine;

uint32_t stackFree(TaskHandle_t task) {
  return task == nullptr ? 0U
                         : static_cast<uint32_t>(uxTaskGetStackHighWaterMark(task)) *
                               sizeof(StackType_t);
}

// Uma vez por minuto. Heap fragmentado demais para o TLS por 3 min seguidos
// -> reinicio limpo com o radio quieto (a fila esta no LittleFS).
void healthCheck() {
  const uint32_t now = millis();
  if (now - gLastHealthMs < 60000U) return;
  gLastHealthMs = now;
  if (now > kHealthyUptimeMs && gBoot.abnormal_resets != 0) gBoot.abnormal_resets = 0;
  const uint32_t block = ESP.getMaxAllocHeap();
  gLowHeapChecks = block < kMinHeapBlockBytes ? static_cast<uint8_t>(gLowHeapChecks + 1) : 0;
  if (gLowHeapChecks >= kLowHeapChecksBeforeRestart &&
      now - radio::gLastActivityMs > kUplinkSettleMs) {
    Serial.printf("[saude] maior bloco livre %lu B < %lu B por 3 min: reiniciando\n",
                  static_cast<unsigned long>(block), static_cast<unsigned long>(kMinHeapBlockBytes));
    Serial.flush();
    ESP.restart();
  }
}

void printHelp() {
  Serial.println(
      "Comandos:\n"
      "  show                         estado e configuracao (sem o token)\n"
      "  set wifi <ssid> <senha>      (SSID sem espacos)\n"
      "  set url <https://.../v1>     base da API\n"
      "  set gw <id>                  ex.: GW01\n"
      "  set token <token>            Bearer desta central\n"
      "  queue                        itens pendentes / em quarentena\n"
      "  radio                        confere o modulo LoRa por AT (sem gravar)\n"
      "  radio provision              aplica UART/LEVEL alvo e confere\n"
      "  reboot"
#ifdef BEE_GATEWAY_FAULT_INJECTION
      "\n  fault corrupt <n> | fault dropack <n> | fault reboot <0|1> | fault drop200 <n>"
#endif
  );
}

void show() {
  const CloudConfig c = loadCloudConfig();
  const radio::Stats r = radio::gStats;
  const radio::Info ri = radio::gInfo;
  const uplink::Stats u = uplink::gStats;
  Serial.printf("fw=%04X heap=%lu maior_bloco=%lu fs_livre=%lu pilha_livre radio=%lu uplink=%lu\n",
                kFirmwareVersion, static_cast<unsigned long>(ESP.getFreeHeap()),
                static_cast<unsigned long>(ESP.getMaxAllocHeap()),
                static_cast<unsigned long>(store::freeBytes()),
                static_cast<unsigned long>(stackFree(radio::gTask)),
                static_cast<unsigned long>(stackFree(uplink::gTask)));
  Serial.printf("cfg: ssid=%s url=%s gw=%s token=%s\n",
                c.wifi_ssid.length() ? c.wifi_ssid.c_str() : "(vazio)",
                c.api_base_url.length() ? c.api_base_url.c_str() : "(vazio)",
                c.gateway_id.length() ? c.gateway_id.c_str() : "(vazio)",
                c.token.length() ? "(definido)" : "(vazio)");
  Serial.printf("lora: at=%s uart=%lu level=%d (alvo %d) channel=%d switch=%d m0m1=%s gravacoes=%s\n",
                ri.at_ok ? "ok" : "sem resposta", static_cast<unsigned long>(ri.uart_baud), ri.level,
                kLevel, ri.channel, ri.switch_mode, ri.pin_control ? "LOW" : "entradas",
                gRadioWritesAllowed ? "permitidas" : "bloqueadas neste boot");
  Serial.printf("radio: frames=%lu stored=%lu status=%lu dup=%lu gaps=%lu crc=%lu acks=%lu "
                "hello=%lu persist_fail=%lu node_writes=%lu max_handle_us=%lu\n",
                static_cast<unsigned long>(r.frames_ok), static_cast<unsigned long>(r.records_stored),
                static_cast<unsigned long>(r.status_stored),
                static_cast<unsigned long>(r.duplicate_records),
                static_cast<unsigned long>(r.gap_frames), static_cast<unsigned long>(r.crc_failures),
                static_cast<unsigned long>(r.acks_sent), static_cast<unsigned long>(r.hello_seen),
                static_cast<unsigned long>(r.persist_failures),
                static_cast<unsigned long>(r.node_writes),
                static_cast<unsigned long>(r.max_handle_us));
  Serial.printf("uplink: wifi=%d ntp=%d suspended=%d ok=%lu fail=%lu http=%d remote_stored=%lu "
                "dup=%lu conflicts=%lu quarantined=%lu heap_tls=%lu\n",
                u.wifi_connected, u.ntp_valid, u.suspended, static_cast<unsigned long>(u.uploads_ok),
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
  const bool ok = saveCloudValue(key, value);
  Serial.printf("%s %s\n", label, ok ? "gravado na NVS" : "FALHOU");
  uplink::gReloadConfig = true;
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
                  static_cast<unsigned long>(store::pendingCount()),
                  static_cast<unsigned long>(store::quarantineCount()));
  } else if (line == "radio") {
    Serial.println("conferindo o modulo (a recepcao pausa alguns segundos)...");
    radio::gCheckRequest = 0;
  } else if (line == "radio provision") {
    Serial.println("provisionando o modulo (a recepcao pausa alguns segundos)...");
    radio::gCheckRequest = 1;
  } else if (line == "reboot") {
    ESP.restart();
  } else if (line.startsWith("set wifi ")) {
    const String rest = line.substring(9);
    const int space = rest.indexOf(' ');
    if (space < 0) {
      Serial.println("uso: set wifi <ssid> <senha>");
    } else {
      saveCloudValue("ssid", rest.substring(0, space));
      saveCloudValue("pass", rest.substring(space + 1));
      Serial.println("wifi gravado na NVS");
      uplink::gReloadConfig = true;
    }
  } else if (line.startsWith("set url ")) {
    setConfig("url", line.substring(8), "url");
  } else if (line.startsWith("set gw ")) {
    setConfig("gw", line.substring(7), "gateway_id");
  } else if (line.startsWith("set token ")) {
    setConfig("token", line.substring(10), "token");  // nunca ecoado
#ifdef BEE_GATEWAY_FAULT_INJECTION
  } else if (line.startsWith("fault corrupt ")) {
    radio::gCorruptEvery = line.substring(14).toInt();
  } else if (line.startsWith("fault dropack ")) {
    radio::gDropAcks = line.substring(14).toInt();
  } else if (line.startsWith("fault reboot ")) {
    radio::gRebootAfterReceive = line.substring(13).toInt() != 0;
  } else if (line.startsWith("fault drop200 ")) {
    uplink::gDropHttp200 = line.substring(14).toInt();
#endif
  } else {
    printHelp();
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("[BeeCooler gateway] fw %04X (producao, arquivo unico)\n", kFirmwareVersion);

  const esp_reset_reason_t reason = esp_reset_reason();
  if (gBoot.magic != kBootGuardMagic || reason == ESP_RST_POWERON) {
    gBoot = {kBootGuardMagic, 0, 0, 0};
  }
  if (abnormalReset(reason) && gBoot.abnormal_resets < 255) ++gBoot.abnormal_resets;
  gBoot.last_reason = static_cast<uint8_t>(reason);
  gRadioWritesAllowed = reason != ESP_RST_BROWNOUT && gBoot.abnormal_resets < kNoWritesAfterResets;
  if (!gRadioWritesAllowed) {
    Serial.printf("[boot] reset=%u (%u anormais seguidos): nada sera gravado no modulo neste boot\n",
                  static_cast<unsigned>(reason), gBoot.abnormal_resets);
  }
  watchdogConfigure();

  if (!store::begin()) {
    // Sem LittleFS a central nao pode confirmar nada: fica sem ACK (o no
    // guarda tudo no ring) e tenta de novo em 1 min.
    Serial.println("LittleFS FALHOU: a central nao pode persistir; reiniciando em 60 s");
    delay(60000);
    ESP.restart();
  }
  Serial.printf("fila: %lu itens pendentes\n", static_cast<unsigned long>(store::pendingCount()));

  radio::start();
  uplink::start();
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
  healthCheck();
  delay(10);
}
