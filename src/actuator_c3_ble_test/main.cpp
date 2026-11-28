// BeeCooler - teste de chaveamento do atuador (ESP32-C3) comandado por BLE.
//
// Firmware de ensaio, independente do S3 e do ESP-NOW: o celular manda
// "ligar por X minutos" e o proprio C3 desliga pelo timer local. Serve para
// validar o chaveamento GPIO6 -> driver -> bomba e medir a resposta da T_in.
//
// BLE: Nordic UART Service (NUS). Apps: nRF Connect, Bluefruit Connect (UART),
// Serial Bluetooth Terminal (Android). Escreva texto na caracteristica RX e
// ative as notificacoes da TX para ver as respostas.
//
// Comandos (BLE ou USB serial, sem diferenca de maiusculas):
//   ON <min>     liga por <min> minutos (1..kMaxOnMinutes)
//   ONS <s>      liga por <s> segundos (1..kMaxOnTestSeconds), teste rapido
//   OFF          desliga agora
//   STATUS       estado atual e tempo restante
//   LOG          ultimos acionamentos (tempos em uptime do C3)
//   BOIA USAR / BOIA IGNORAR   liga/desliga o bloqueio pela boia
//   HELP         lista de comandos
//
// Circuito: eletric_schema/atuador*.png (BC-ACT-001/002 rev. B). GPIO6 -> driver
// BC337/BC327 -> gate do IRF3205; GPIO7 = boia (INPUT_PULLUP, fechada = agua
// OK); GPIO3 = tensao da bateria (sensor 0-25 V com R7, fator nominal 8).
//
// Seguranca: GPIO6 no nivel "desligado" antes de qualquer outra inicializacao; teto de
// duracao aplicado aqui no C3; perder a conexao BLE NAO desliga nem estende o
// acionamento, ele so termina no prazo. Sem pareamento/senha: qualquer pessoa
// por perto com um app BLE consegue comandar. Aceitavel so para este ensaio.

#include <Arduino.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>

namespace {

// Pinagem do atuador (mesma do actuator_c3_test). O nivel que liga depende da
// topologia do driver: no estagio nao inversor (BC337 puxa a base do BC327, que
// leva o gate do IRF3205 a 12 V) HIGH liga. Num totem-pole com inversor na
// entrada, LOW liga: compile com -DBEE_DRIVER_ACTIVE_HIGH=0.
constexpr uint8_t kPinDriver = 6;
#ifndef BEE_DRIVER_ACTIVE_HIGH
#define BEE_DRIVER_ACTIVE_HIGH 1
#endif
constexpr uint8_t kLevelOn = BEE_DRIVER_ACTIVE_HIGH ? HIGH : LOW;
constexpr uint8_t kLevelOff = BEE_DRIVER_ACTIVE_HIGH ? LOW : HIGH;

// Boia S1: contato para GND, fechado com agua. Com o bloqueio ativo, aberta (ou
// fio solto) impede ligar. Padrao 0: a boia ainda nao esta montada, entao o
// estado e so lido e informado. Ative com BOIA USAR ou -DBEE_FLOAT_INTERLOCK=1.
constexpr uint8_t kPinFloat = 7;
#ifndef BEE_FLOAT_INTERLOCK
#define BEE_FLOAT_INTERLOCK 0
#endif
constexpr uint32_t kFloatTripMs = 2000;  // ignora marola curta com a bomba ligada

// Bateria: divisor do modulo 30k/7.5k com R7 = 10k em paralelo -> fator ~8
// (14,4 V -> 1,8 V). Nominal; calibrar contra o multimetro.
constexpr uint8_t kPinVbat = 3;
#ifndef BEE_VBAT_FACTOR
#define BEE_VBAT_FACTOR 8.0f
#endif
constexpr float kVbatFactor = BEE_VBAT_FACTOR;

#ifndef BEE_BLE_MAX_ON_MINUTES
#define BEE_BLE_MAX_ON_MINUTES 20
#endif
constexpr uint32_t kMaxOnMinutes = BEE_BLE_MAX_ON_MINUTES;
constexpr uint32_t kMaxOnTestSeconds = 120;
static_assert(kMaxOnMinutes >= 1 && kMaxOnMinutes <= 30, "Teto do ensaio: 30 min");

constexpr char kDeviceName[] = "BeeCooler-C3";
constexpr char kNusService[] = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr char kNusRx[] = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";  // celular -> C3
constexpr char kNusTx[] = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";  // C3 -> celular
constexpr size_t kBleChunk = 20;  // cabe no MTU padrao (23 B) sem negociar MTU

constexpr size_t kCommandMax = 32;
constexpr size_t kHistoryCapacity = 16;

enum class EndReason : uint8_t { kRunning, kTimer, kOffCommand, kReplaced, kFloat };

struct Episode {
  uint32_t start_ms;
  uint32_t requested_ms;
  uint32_t executed_ms;
  EndReason end;
};

Episode gHistory[kHistoryCapacity];
uint32_t gEpisodeCount = 0;  // total desde o boot; gHistory guarda os ultimos

bool gFloatBypass = !BEE_FLOAT_INTERLOCK;
uint32_t gFloatOpenSinceMs = 0;

bool gOn = false;
uint32_t gOnStartMs = 0;
uint32_t gOnDurationMs = 0;

BLECharacteristic* gTx = nullptr;
volatile bool gConnected = false;

// O callback BLE so copia o texto; o comando e executado no loop().
portMUX_TYPE gRxMux = portMUX_INITIALIZER_UNLOCKED;
char gRxBuffer[kCommandMax + 1];
volatile bool gRxPending = false;

const char* endName(EndReason r) {
  switch (r) {
    case EndReason::kRunning: return "ligado";
    case EndReason::kTimer: return "timer";
    case EndReason::kOffCommand: return "OFF";
    case EndReason::kReplaced: return "novo ON";
    case EndReason::kFloat: return "boia (sem agua)";
  }
  return "?";
}

// Resposta vai para a serial e, se houver conexao, em notificacoes de 20 B.
void reply(const char* fmt, ...) {
  char text[192];
  va_list args;
  va_start(args, fmt);
  vsnprintf(text, sizeof(text), fmt, args);
  va_end(args);
  Serial.println(text);
  if (!gConnected || gTx == nullptr) return;
  const size_t len = strlen(text);
  for (size_t i = 0; i < len; i += kBleChunk) {
    const size_t n = min(kBleChunk, len - i);
    gTx->setValue(reinterpret_cast<uint8_t*>(text + i), n);
    gTx->notify();
    delay(10);  // da tempo da pilha BLE despachar cada pedaco
  }
  const uint8_t newline = '\n';
  gTx->setValue(const_cast<uint8_t*>(&newline), 1);
  gTx->notify();
}

bool floatHasWater() { return digitalRead(kPinFloat) == LOW; }

float readVbat() {
  uint32_t sum_mv = 0;
  for (int i = 0; i < 8; ++i) sum_mv += analogReadMilliVolts(kPinVbat);
  return (sum_mv / 8.0f) * kVbatFactor / 1000.0f;
}

Episode& currentEpisode() { return gHistory[(gEpisodeCount - 1) % kHistoryCapacity]; }

void driverOff(EndReason reason) {
  digitalWrite(kPinDriver, kLevelOff);  // primeiro o hardware, depois o registro
  if (!gOn) return;
  gOn = false;
  Episode& e = currentEpisode();
  e.executed_ms = millis() - gOnStartMs;
  e.end = reason;
  reply("[OFF] bomba desligada motivo=%s ligado=%lu s", endName(reason),
        static_cast<unsigned long>(e.executed_ms / 1000));
}

void driverOn(uint32_t duration_ms) {
  if (!gFloatBypass && !floatHasWater()) {
    reply("[BLOQUEADO] boia aberta: sem agua (ou boia desconectada)");
    return;
  }
  if (gOn) driverOff(EndReason::kReplaced);
  gFloatOpenSinceMs = 0;
  gOnStartMs = millis();
  gOnDurationMs = duration_ms;
  gOn = true;
  Episode& e = gHistory[gEpisodeCount % kHistoryCapacity];
  e = {gOnStartMs, duration_ms, 0, EndReason::kRunning};
  ++gEpisodeCount;
  digitalWrite(kPinDriver, kLevelOn);
  reply("[ON] bomba ligada por %lu s (uptime %lu s)", static_cast<unsigned long>(duration_ms / 1000),
        static_cast<unsigned long>(gOnStartMs / 1000));
}

void printStatus() {
  const uint32_t now = millis();
  if (gOn) {
    const uint32_t elapsed = now - gOnStartMs;
    reply("[STATUS] LIGADO ha %lu s, faltam %lu s (uptime %lu s)",
          static_cast<unsigned long>(elapsed / 1000),
          static_cast<unsigned long>((gOnDurationMs - elapsed) / 1000),
          static_cast<unsigned long>(now / 1000));
  } else {
    reply("[STATUS] DESLIGADO, %lu acionamentos (uptime %lu s)",
          static_cast<unsigned long>(gEpisodeCount), static_cast<unsigned long>(now / 1000));
  }
  reply("[STATUS] boia=%s%s bateria=%.2f V (nao calibrado)", floatHasWater() ? "agua" : "ABERTA",
        gFloatBypass ? " (ignorada)" : "", readVbat());
}

void printLog() {
  if (gEpisodeCount == 0) {
    reply("[LOG] nenhum acionamento");
    return;
  }
  const uint32_t n = min<uint32_t>(gEpisodeCount, kHistoryCapacity);
  for (uint32_t k = gEpisodeCount - n; k < gEpisodeCount; ++k) {
    const Episode& e = gHistory[k % kHistoryCapacity];
    const uint32_t executed = e.end == EndReason::kRunning ? millis() - e.start_ms : e.executed_ms;
    reply("[LOG] #%lu inicio=%lus pedido=%lus feito=%lus fim=%s", static_cast<unsigned long>(k + 1),
          static_cast<unsigned long>(e.start_ms / 1000),
          static_cast<unsigned long>(e.requested_ms / 1000),
          static_cast<unsigned long>(executed / 1000), endName(e.end));
  }
}

void printHelp() {
  reply("ON <min> (1-%lu) | ONS <s> (1-%lu) | OFF | STATUS | LOG | BOIA USAR/IGNORAR",
        static_cast<unsigned long>(kMaxOnMinutes), static_cast<unsigned long>(kMaxOnTestSeconds));
}

// Le um inteiro positivo apos o comando; devolve 0 se ausente ou invalido.
uint32_t parseArgument(const char* text) {
  while (*text == ' ') ++text;
  if (*text == '\0') return 0;
  char* end = nullptr;
  const long value = strtol(text, &end, 10);
  while (end != nullptr && *end == ' ') ++end;
  if (end == nullptr || *end != '\0' || value <= 0) return 0;
  return static_cast<uint32_t>(value);
}

void handleCommand(char* line) {
  // Normaliza: tira \r\n e espacos das pontas, passa para maiusculas.
  size_t len = strlen(line);
  while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n' || line[len - 1] == ' ')) {
    line[--len] = '\0';
  }
  while (*line == ' ') ++line;
  for (char* p = line; *p; ++p) *p = static_cast<char>(toupper(static_cast<unsigned char>(*p)));
  if (*line == '\0') return;

  if (strncmp(line, "ONS", 3) == 0 && (line[3] == ' ' || line[3] == '\0')) {
    const uint32_t s = parseArgument(line + 3);
    if (s == 0 || s > kMaxOnTestSeconds) {
      reply("[ERRO] use ONS 1..%lu", static_cast<unsigned long>(kMaxOnTestSeconds));
      return;
    }
    driverOn(s * 1000UL);
  } else if (strncmp(line, "ON", 2) == 0 && (line[2] == ' ' || line[2] == '\0')) {
    const uint32_t m = parseArgument(line + 2);
    if (m == 0 || m > kMaxOnMinutes) {
      reply("[ERRO] use ON 1..%lu (minutos)", static_cast<unsigned long>(kMaxOnMinutes));
      return;
    }
    driverOn(m * 60000UL);
  } else if (strcmp(line, "OFF") == 0) {
    if (gOn) {
      driverOff(EndReason::kOffCommand);
    } else {
      digitalWrite(kPinDriver, kLevelOff);
      reply("[OFF] ja estava desligado");
    }
  } else if (strcmp(line, "STATUS") == 0) {
    printStatus();
  } else if (strcmp(line, "LOG") == 0) {
    printLog();
  } else if (strcmp(line, "BOIA IGNORAR") == 0) {
    gFloatBypass = true;
    reply("[BOIA] bloqueio desativado (so leitura)");
  } else if (strcmp(line, "BOIA USAR") == 0) {
    gFloatBypass = false;
    reply("[BOIA] bloqueio ativo");
  } else if (strcmp(line, "HELP") == 0) {
    printHelp();
  } else {
    reply("[ERRO] comando desconhecido: %s", line);
    printHelp();
  }
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    gConnected = true;
    Serial.println("[BLE] conectado");
  }
  void onDisconnect(BLEServer* server) override {
    gConnected = false;
    Serial.println("[BLE] desconectado; acionamento em curso continua ate o prazo");
    server->startAdvertising();  // permite reconectar
  }
};

class RxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    const String value = characteristic->getValue();  // Arduino-ESP32 3.x: String
    portENTER_CRITICAL(&gRxMux);
    if (!gRxPending) {  // um comando por vez; o seguinte espera o loop consumir
      const size_t n = min(static_cast<size_t>(value.length()), kCommandMax);
      memcpy(gRxBuffer, value.c_str(), n);
      gRxBuffer[n] = '\0';
      gRxPending = true;
    }
    portEXIT_CRITICAL(&gRxMux);
  }
};

void startBle() {
  BLEDevice::init(kDeviceName);
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  BLEService* service = server->createService(kNusService);
  gTx = service->createCharacteristic(kNusTx, BLECharacteristic::PROPERTY_NOTIFY);
#if !CONFIG_BT_NIMBLE_ENABLED
  gTx->addDescriptor(new BLE2902());  // NimBLE cria o 2902 sozinho; Bluedroid nao
#endif
  BLECharacteristic* rx = service->createCharacteristic(
      kNusRx, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  rx->setCallbacks(new RxCallbacks());
  service->start();
  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(kNusService);
  advertising->setScanResponse(true);
  BLEDevice::startAdvertising();
}

char gSerialLine[kCommandMax + 1];
size_t gSerialLen = 0;

void pollSerial() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      gSerialLine[gSerialLen] = '\0';
      if (gSerialLen > 0) handleCommand(gSerialLine);
      gSerialLen = 0;
    } else if (gSerialLen < kCommandMax) {
      gSerialLine[gSerialLen++] = c;
    }
  }
}

}  // namespace

void setup() {
  // Saida desligada antes de tudo (o pino so e controlado depois do boot da ROM;
  // ate la quem segura o gate em 0 V sao R2 e R6 do esquema).
  pinMode(kPinDriver, OUTPUT);
  digitalWrite(kPinDriver, kLevelOff);

  pinMode(kPinFloat, INPUT_PULLUP);
  analogSetPinAttenuation(kPinVbat, ADC_11db);  // atenuacao maxima (esquema, nota 04)

  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("[C3] BeeCooler actuator_c3_ble_test");
  startBle();
  Serial.printf("[C3] BLE '%s' anunciando (NUS); GPIO6 ativo em %s, agora desligado; teto %lu min\n",
                kDeviceName, BEE_DRIVER_ACTIVE_HIGH ? "HIGH" : "LOW",
                static_cast<unsigned long>(kMaxOnMinutes));
  printHelp();
  printStatus();
}

void loop() {
  if (gOn && millis() - gOnStartMs >= gOnDurationMs) driverOff(EndReason::kTimer);

  // Boia aberta por mais de kFloatTripMs com a bomba ligada: desliga.
  if (gOn && !gFloatBypass && !floatHasWater()) {
    if (gFloatOpenSinceMs == 0) gFloatOpenSinceMs = millis() | 1U;
    else if (millis() - gFloatOpenSinceMs >= kFloatTripMs) driverOff(EndReason::kFloat);
  } else {
    gFloatOpenSinceMs = 0;
  }

  char command[kCommandMax + 1];
  bool have = false;
  portENTER_CRITICAL(&gRxMux);
  if (gRxPending) {
    memcpy(command, gRxBuffer, sizeof(command));
    gRxPending = false;
    have = true;
  }
  portEXIT_CRITICAL(&gRxMux);
  if (have) handleCommand(command);

  pollSerial();
  delay(20);
}
