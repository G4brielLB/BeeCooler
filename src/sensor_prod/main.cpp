// BeeCooler - firmware de PRODUCAO do no sensor (ESP32-S3), arquivo unico.
//
// Mesmo formato do comunicacao_s3_test: um main.cpp que usa os drivers de
// src/sensor (SHT30, ADXL345, SPI, ring na flash, microSD, bateria) e as
// bibliotecas comuns de lib/. Implementa o ciclo de producao da Arquitetura
// v1.2 com as correcoes do relatorio da AP2 (secao 8.2, passo 2, e Tabela 12):
//
//   - radio: le o DX-LR32 por AT no cold boot e acha a UART (9600 de fabrica
//     ou 115200); provisiona UART 115200 e LEVEL 3 conferindo por leitura de
//     volta; so comanda M0/M1 com SWITCH=1 lido do modulo;
//   - espera pelo ACK do LEVEL ou medida (console `rtt`, p99 + 30 %);
//   - registros por quadro limitados a 400 ms no ar;
//   - bateria sem circuito -> sentinela 0xFFFF; politica de energia fixa no
//     nivel normal;
//   - hora valida so entre a compilacao e 10 anos depois (o 2099 da bancada).
//
// Ciclo (setup() faz tudo; loop() nunca roda):
//   boot -> radio (AT so no cold boot) -> DS1302 -> SHT30 -> janela ADXL345 ->
//   features -> microSD -> ring + RAM do DS1302 -> LoRa (se for o slot) ->
//   deep sleep ancorado no proximo slot.
//
// ---- Protecoes de pilha e de hardware (revisadas para este arquivo) --------
//   Pilha: a loopTask roda com 16 KB (o padrao e 8 KB, e foi o estouro de 8 KB
//     no Welch que reiniciava o no na bancada). Buffers grandes ficam fora da
//     pilha: janela de 96 kB e amostras do `rtt` no heap, FFT estatica na
//     BeeCoolerVib, quadros e registros da sessao estaticos. O pico de pilha e
//     impresso a cada ciclo ([stack]) e no `status` do console.
//   M0/M1: com SWITCH=0 sao saidas do modulo; o ESP os deixa como entrada.
//     So os dirige com SWITCH=1 lido de volta. Trocar o SWITCH para 1 exige
//     -DBEE_LORA_SWITCH1=1 e resistor de 1 kohm em serie em M0 e M1.
//   DS1302: driver proprio neste arquivo; solta a linha IO antes da borda em
//     que o DS1302 comeca a responder a uma leitura (o driver de src/sensor a
//     soltava microssegundos depois, com as duas saidas ligadas entre si).
//   Rele de reinicio: se o no reiniciar 3 vezes seguidas por panic, watchdog
//     ou brownout, dorme um ciclo inteiro sem radio antes de tentar de novo
//     (sem laco de boot drenando a bateria e transmitindo). Depois de um
//     brownout nao grava nada no modulo e nao manda HELLO nesse boot.
//   Watchdog de tarefa (300 s): qualquer espera que trave reinicia o no em
//     vez de deixa-lo acordado ate a bateria acabar.
//   Console: sai sozinho apos 5 min sem digitar (um byte perdido na serial
//     nao prende o no acordado).
//   Gravacoes no modulo: so o que difere do alvo, e no maximo 3 tentativas
//     falhas (memoria do modulo); `radio provision` no console zera a conta.
//   Antena: o firmware nao detecta antena. Nunca ligue o no ou a central sem
//     antena: o SX1262 transmite a 22 dBm no HELLO do boot.

#include <Arduino.h>
#include <Preferences.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <esp_idf_version.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

#include <BeeCoolerLink.h>
#include <BeeCoolerLinkBudget.h>
#include <BeeCoolerLogic.h>
#include <BeeCoolerLora.h>
#include <BeeCoolerLoraAt.h>
#include <BeeCoolerRecord.h>
#include <BeeCoolerVib.h>

#include "../sensor/Adxl345Fifo.h"
#include "../sensor/BatteryMonitor.h"
#include "../sensor/FlashRing.h"
#include "../sensor/RawSink.h"
#include "../sensor/SharedSpi.h"
#include "../sensor/Sht30Pair.h"
#include "../sensor/config.h"
#include "../sensor/pins.h"

using namespace BeeCoolerRecord;
using namespace BeeCoolerLink;
using BeeCoolerLogic::LevelParams;

#ifdef SET_LOOP_TASK_STACK_SIZE
SET_LOOP_TASK_STACK_SIZE(16 * 1024);
#endif

// =============================================================================
// Configuracao de producao (o resto vem de ../sensor/config.h)
// =============================================================================

#ifndef BEE_LORA_AUTOPROVISION
#define BEE_LORA_AUTOPROVISION 1
#endif
#ifndef BEE_LORA_LEVEL
#define BEE_LORA_LEVEL 3  // igual ao da central (src/gateway_prod/config.h)
#endif
#ifndef BEE_LORA_SWITCH1
#define BEE_LORA_SWITCH1 0  // so com resistor de 1 kohm em serie em M0 e M1
#endif
#ifndef BEE_VBAT_CIRCUIT
#define BEE_VBAT_CIRCUIT 0  // circuito de medicao da bateria montado e calibrado
#endif
#ifndef BEE_POWER_POLICY
#define BEE_POWER_POLICY 0  // politica por niveis (so com o circuito)
#endif

namespace prod {

constexpr uint16_t kFirmwareVersion = 0x0110;  // 1.1: producao (HELLO/STATUS)
constexpr uint8_t kNodeId = BEE_NODE_ID;

// Radio [R 8.2 passo 2; Tabela 11]
constexpr bool kAutoProvision = BEE_LORA_AUTOPROVISION != 0;
constexpr uint32_t kTargetBaud = 115200U;
constexpr uint32_t kFactoryBaud = 9600U;   // BAUD=3, confirmado na bancada
constexpr int kLevel = BEE_LORA_LEVEL;     // LEVEL 3 = SF8/250 kHz
constexpr bool kSwitch1 = BEE_LORA_SWITCH1 != 0;
// Codigos AT+BAUD tentados para 115200 (nao confirmados no guia do LR32;
// o resultado e conferido varrendo a UART depois do reinicio do modulo).
constexpr uint8_t kBaudCodes115200[] = {7U, 8U};
constexpr uint8_t kMaxProvisionFailures = 3U;

constexpr uint32_t kMaxAirtimeMs = 400U;     // teto por transmissao [R 5.1]
constexpr uint16_t kAckMinMs = 300U;
constexpr uint16_t kAckMaxMs = 5000U;
constexpr uint16_t kRttMaxSamples = 500U;
constexpr uint32_t kRttSampleTimeoutMs = 3000U;
constexpr uint8_t kRecheckAfterFailedSessions = 3U;

// Bateria e energia [R 4.4.4, Tabela 12]
constexpr bool kVbatCircuit = BEE_VBAT_CIRCUIT != 0;
constexpr bool kPowerPolicy = kVbatCircuit && BEE_POWER_POLICY != 0;

// Relogio [R Tabela 12]
constexpr uint32_t kClockMaxAheadS = 10U * 366U * 24U * 3600U;

// Protecoes
constexpr uint32_t kWatchdogS = 300U;
constexpr uint8_t kSafeModeAfterResets = 3U;
constexpr uint32_t kConsoleIdleMs = 5U * 60U * 1000U;

static_assert(kNodeId >= 1 && kNodeId <= 8,
              "BEE_NODE_ID 1..8: o slot (20 s + 30 s por no) cabe no ciclo e no watchdog");
static_assert(kLevel >= 0 && kLevel <= 7, "BEE_LORA_LEVEL 0..7");

}  // namespace prod

namespace {

// =============================================================================
// Estado
// =============================================================================

// Mantido na RTC durante o deep sleep (Arquitetura 5.4). Nao sobrevive a uma
// queda de energia: o DS1302 e o ring na flash sao as copias duraveis.
struct NodeState {
  uint32_t magic;
  uint32_t last_seq;
  uint32_t confirmed_seq;
  uint32_t last_sync_epoch;
  uint32_t last_lora_slot;
  uint32_t cycles_since_sd_refresh;
  uint32_t lora_baud;
  uint16_t boot_count;
  uint16_t cycles_since_lora;
  uint16_t frame_seq;
  uint16_t sd_free_mb;
  uint16_t vbat_tx_mv;
  uint16_t vbat_history[3];
  uint16_t ack_timeout_ms;
  uint16_t pending_flags;
  uint8_t vbat_history_count;
  uint8_t sd_errors;
  uint8_t failed_sessions;
  uint8_t last_ack_rssi;
  int8_t lora_level;
  uint8_t lora_pin_control;
  uint8_t records_per_frame;
  uint8_t radio_at_ok;
  uint8_t radio_recheck;
  BeeCoolerLogic::PowerState power;
  BeeCoolerLogic::StuckHistory stuck;
};
constexpr uint32_t kNodeStateMagic = 0xBEE0C003U;
RTC_DATA_ATTR NodeState gState;

// Sobrevive a resets por software, panic e watchdog (nao a falta de energia).
struct BootGuard {
  uint32_t magic;
  uint8_t abnormal_resets;  // seguidos, sem um ciclo completo no meio
  uint8_t last_reason;
  uint16_t reserved;
};
constexpr uint32_t kBootGuardMagic = 0xB007AB1EU;
RTC_NOINIT_ATTR BootGuard gBoot;

Preferences gPrefs;  // namespace "bee" (o mesmo do BatteryMonitor)
FlashRing gRing;
BeeCoolerLora gLora;
Sht30Pair gSht;
Adxl345Fifo gAdxl;
StreamParser gParser;

uint32_t buildEpoch() {
  static uint32_t cached = 0;
  if (cached == 0) BeeCoolerLogic::parseBuildEpoch(__DATE__, __TIME__, cached);
  return cached;
}

bool plausibleEpoch(uint32_t epoch) {
  return BeeCoolerLinkBudget::plausibleEpoch(epoch, buildEpoch(), prod::kClockMaxAheadS);
}

// =============================================================================
// Watchdog de tarefa
// =============================================================================

void watchdogStart() {
#if ESP_IDF_VERSION_MAJOR >= 5
  const esp_task_wdt_config_t cfg = {prod::kWatchdogS * 1000U, 0U, true};
  if (esp_task_wdt_reconfigure(&cfg) != ESP_OK) esp_task_wdt_init(&cfg);
#else
  esp_task_wdt_init(prod::kWatchdogS, true);  // IDF 4: reconfigura se ja ativo
#endif
  esp_task_wdt_add(nullptr);
}

inline void feed() { esp_task_wdt_reset(); }

// =============================================================================
// DS1302 (3 fios) com a linha IO sem disputa
// =============================================================================

struct __attribute__((packed)) DsState {
  uint8_t magic;
  uint8_t layout;
  uint16_t boot_count;
  uint32_t last_seq;
  uint32_t confirmed_seq;
  uint32_t last_sync_epoch;
  uint8_t power_level;
  uint8_t state_flags;
  uint8_t reserved[12];
  uint8_t crc8;
};
static_assert(sizeof(DsState) == 31, "RAM do DS1302: 31 bytes");
constexpr uint8_t kDsMagic = 0xBC;
constexpr uint8_t kDsLayout = 1;

class Ds1302 {
 public:
  void begin(int ce, int io, int sclk) {
    ce_ = ce;
    io_ = io;
    sclk_ = sclk;
    pinMode(io_, INPUT);
    pinMode(ce_, OUTPUT);
    pinMode(sclk_, OUTPUT);
    digitalWrite(ce_, LOW);
    digitalWrite(sclk_, LOW);
  }

  // Calendario valido e oscilador rodando (CH = 0, modo 24 h).
  bool readEpoch(uint32_t& epoch) {
    uint8_t r[8];
    readClockRegisters(r);
    if (r[0] & 0x80) return false;
    if (r[2] & 0x80) return false;
    return BeeCoolerLogic::civilToEpoch(2000 + bcd(r[6]), bcd(r[4] & 0x1F), bcd(r[3] & 0x3F),
                                        bcd(r[2] & 0x3F), bcd(r[1] & 0x7F), bcd(r[0] & 0x7F),
                                        epoch);
  }

  void readClockRegisters(uint8_t (&r)[8]) {
    start();
    readCommand(0xBF);  // burst de leitura do calendario
    for (uint8_t i = 0; i < 8; ++i) r[i] = readByte();
    end();
  }

  void writeEpoch(uint32_t epoch) {
    int y, mo, d, h, mi, s;
    BeeCoolerLogic::epochToCivil(epoch, y, mo, d, h, mi, s);
    writeRegister(0x07, 0x00);  // tira a protecao de escrita
    start();
    writeByte(0xBE);            // burst de escrita do calendario
    writeByte(toBcd(s));        // CH = 0: oscilador ligado
    writeByte(toBcd(mi));
    writeByte(toBcd(h));        // modo 24 h
    writeByte(toBcd(d));
    writeByte(toBcd(mo));
    writeByte(1);               // dia da semana, nao usado
    writeByte(toBcd(y - 2000));
    writeByte(0x00);
    end();
    writeRegister(0x07, 0x80);
  }

  bool loadState(DsState& state) {
    uint8_t raw[sizeof(DsState)];
    start();
    readCommand(0xFF);  // burst de leitura da RAM
    for (size_t i = 0; i < sizeof(raw); ++i) raw[i] = readByte();
    end();
    memcpy(&state, raw, sizeof(state));
    return state.magic == kDsMagic && state.layout == kDsLayout &&
           state.crc8 == crc8(raw, sizeof(raw) - 1);
  }

  void saveState(DsState& state) {
    state.magic = kDsMagic;
    state.layout = kDsLayout;
    state.crc8 = crc8(reinterpret_cast<const uint8_t*>(&state), sizeof(state) - 1);
    writeRegister(0x07, 0x00);
    start();
    writeByte(0xFE);  // burst de escrita da RAM
    const uint8_t* raw = reinterpret_cast<const uint8_t*>(&state);
    for (size_t i = 0; i < sizeof(state); ++i) writeByte(raw[i]);
    end();
    writeRegister(0x07, 0x80);
  }

 private:
  static int bcd(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
  static uint8_t toBcd(int v) { return static_cast<uint8_t>(((v / 10) << 4) | (v % 10)); }

  void start() {
    pinMode(io_, INPUT);
    digitalWrite(sclk_, LOW);
    digitalWrite(ce_, HIGH);
    delayMicroseconds(4);  // tCC
  }
  void end() {
    digitalWrite(ce_, LOW);
    pinMode(io_, INPUT);
    delayMicroseconds(4);  // tCWH
  }
  void clockBit(bool release_before_fall) {
    delayMicroseconds(1);
    digitalWrite(sclk_, HIGH);
    delayMicroseconds(1);
    // Num comando de leitura o DS1302 passa a dirigir IO na descida do 8o
    // pulso: o ESP solta a linha antes dessa descida.
    if (release_before_fall) pinMode(io_, INPUT);
    digitalWrite(sclk_, LOW);
    delayMicroseconds(1);
  }
  void writeBits(uint8_t value, bool read_command) {
    digitalWrite(io_, value & 1U);  // nivel antes de virar saida
    pinMode(io_, OUTPUT);
    for (uint8_t i = 0; i < 8; ++i) {  // LSB primeiro, amostrado na subida
      digitalWrite(io_, (value >> i) & 1U);
      clockBit(read_command && i == 7);
    }
    if (!read_command) pinMode(io_, INPUT);
  }
  void writeByte(uint8_t value) { writeBits(value, false); }
  void readCommand(uint8_t command) { writeBits(command, true); }
  uint8_t readByte() {
    uint8_t value = 0;
    for (uint8_t i = 0; i < 8; ++i) {
      if (digitalRead(io_)) value |= static_cast<uint8_t>(1U << i);
      digitalWrite(sclk_, HIGH);
      delayMicroseconds(1);
      digitalWrite(sclk_, LOW);
      delayMicroseconds(1);
    }
    return value;
  }
  void writeRegister(uint8_t reg, uint8_t value) {
    start();
    writeByte(static_cast<uint8_t>(0x80 | (reg << 1)));
    writeByte(value);
    end();
  }

  int ce_ = -1, io_ = -1, sclk_ = -1;
};

Ds1302 gClock;

bool readClock(uint32_t& epoch) { return gClock.readEpoch(epoch) && plausibleEpoch(epoch); }

void saveDsState() {
  DsState ds;
  memset(&ds, 0, sizeof(ds));
  ds.boot_count = gState.boot_count;
  ds.last_seq = gState.last_seq;
  ds.confirmed_seq = gState.confirmed_seq;
  ds.last_sync_epoch = gState.last_sync_epoch;
  ds.power_level = gState.power.level;
  gClock.saveState(ds);
}

// UC-10: reconstroi o estado no cold boot a partir do DS1302 e da flash.
void recoverAfterColdBoot(bool ring_ok) {
  DsState ds;
  const bool ds_valid = gClock.loadState(ds);
  const uint32_t hint = ds_valid ? ds.last_seq : 0;
  const uint32_t last = ring_ok ? gRing.recoverLastSeq(hint) : hint;
  gState.last_seq = last;
  gState.confirmed_seq = ds_valid ? min(ds.confirmed_seq, last) : 0;
  gState.boot_count = ds_valid ? static_cast<uint16_t>(ds.boot_count + 1) : 1;
  gState.power.level = (prod::kPowerPolicy && ds_valid) ? (ds.power_level & 0x3) : 0;
  gState.last_sync_epoch = ds_valid ? ds.last_sync_epoch : 0;
  gState.vbat_tx_mv = kInvalidU16;
  gState.pending_flags |= kColdBoot;
  saveDsState();
}

// =============================================================================
// Rele de reinicio
// =============================================================================

bool abnormalReset(esp_reset_reason_t r) {
  return r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT ||
         r == ESP_RST_WDT || r == ESP_RST_BROWNOUT;
}

// =============================================================================
// Radio: perfil, AT e NVS
// =============================================================================

BeeCoolerLoraAt::Port loraPort(bool pin_control) {
  return {&Serial1,
          static_cast<int8_t>(kPinLoraRx),
          static_cast<int8_t>(kPinLoraTx),
          static_cast<int8_t>(kPinLoraM0),
          static_cast<int8_t>(kPinLoraM1),
          static_cast<int8_t>(kPinLoraAux),
          pin_control};
}

uint16_t ackOverrideMs() {
  uint16_t v = 0;
  if (gPrefs.begin("bee", true)) {
    v = gPrefs.getUShort("ackms", 0U);
    gPrefs.end();
  }
  return v;
}

void refreshAckTimeout() {
  const uint16_t o = ackOverrideMs();
  uint32_t t = o != 0U ? o
                       : BeeCoolerLinkBudget::defaultAckTimeoutMs(gState.lora_level, kAckSize);
  t = max<uint32_t>(prod::kAckMinMs, min<uint32_t>(prod::kAckMaxMs, t));
  gState.ack_timeout_ms = static_cast<uint16_t>(t);
}

bool setAckOverrideMs(uint16_t ms) {
  if (ms != 0U && (ms < prod::kAckMinMs || ms > prod::kAckMaxMs)) return false;
  if (!gPrefs.begin("bee", false)) return false;
  gPrefs.putUShort("ackms", ms);
  gPrefs.end();
  refreshAckTimeout();
  return true;
}

uint8_t recordsForLevel(int level) {
  const size_t overhead = kHeaderSize + kCrcSize;
  const uint8_t hard = static_cast<uint8_t>(kMaxRecordsPerFrame);
  uint8_t n = BeeCoolerLinkBudget::maxRecordsPerFrame(level, prod::kMaxAirtimeMs * 1000U,
                                                      overhead, kRecordSize, hard);
  if (n == 0U) {  // LEVEL desconhecido: o pior caso conhecido (LEVEL 2)
    n = BeeCoolerLinkBudget::maxRecordsPerFrame(2, prod::kMaxAirtimeMs * 1000U, overhead,
                                                kRecordSize, hard);
  }
  return n == 0U ? 1U : n;
}

void beginLoraDriver() {
  const BeeCoolerLora::Pins pins = {kPinLoraRx, kPinLoraTx, kPinLoraM0, kPinLoraM1,
                                    kPinLoraAux};
  // Com pin control comeca em sleep (M0=M1=1); sem ele, M0/M1 ficam entradas.
  gLora.begin(Serial1, pins, gState.lora_baud != 0U ? gState.lora_baud : prod::kFactoryBaud,
              gState.lora_pin_control != 0U);
}

void printRadioProfile() {
  Serial.printf("[LoRa] perfil: UART %lu Bd, LEVEL %d (alvo %d), M0/M1 %s, %u registro(s)/quadro, "
                "espera do ACK %u ms (%s), AT %s\n",
                static_cast<unsigned long>(gState.lora_baud), gState.lora_level, prod::kLevel,
                gState.lora_pin_control ? "pelo ESP (SWITCH=1)" : "entradas (SWITCH=0)",
                gState.records_per_frame, gState.ack_timeout_ms,
                ackOverrideMs() != 0U ? "medida" : "padrao do LEVEL",
                gState.radio_at_ok ? "ok" : "sem resposta");
}

// AT no cold boot (ou apos sessoes falhas). `allow_writes` = falso depois de
// um brownout: nada e gravado no modulo nesse boot.
bool radioBringUp(bool allow_writes, bool force_provision) {
  uint32_t cached_baud = 0;
  bool cached_pins = false;
  uint8_t failures = 0;
  if (gPrefs.begin("bee", true)) {
    cached_baud = gPrefs.getUInt("lbaud", 0U);
    cached_pins = gPrefs.getBool("lsw1", false);
    failures = gPrefs.getUChar("lprov", 0U);
    gPrefs.end();
  }
  const uint32_t preferred = cached_baud != 0U ? cached_baud : prod::kFactoryBaud;

  BeeCoolerLoraAt::Port port = loraPort(cached_pins);
  BeeCoolerLoraAt::applyPinMode(port);  // LOW/LOW so com SWITCH=1 lembrado
  BeeCoolerLoraAt::Config cfg;
  bool ok = BeeCoolerLoraAt::detect(port, preferred, cfg, &Serial);
  feed();
  if (!ok && cached_pins) {  // modulo trocado ou de volta a SWITCH=0
    port.pin_control = false;
    BeeCoolerLoraAt::applyPinMode(port);
    ok = BeeCoolerLoraAt::detect(port, preferred, cfg, &Serial);
    feed();
  }
  if (!ok && !cached_pins && prod::kSwitch1) {
    // NVS apagada com modulo em SWITCH=1 (dormindo com os pinos soltos).
    // Seguro so com os resistores exigidos pela flag.
    port.pin_control = true;
    BeeCoolerLoraAt::applyPinMode(port);
    ok = BeeCoolerLoraAt::detect(port, preferred, cfg, &Serial);
    if (!ok) {
      port.pin_control = false;
      BeeCoolerLoraAt::applyPinMode(port);
    }
    feed();
  }

  if (ok) {
    port.pin_control = cfg.switch_mode == 1;
    BeeCoolerLoraAt::applyPinMode(port);
    BeeCoolerLoraAt::printConfig(cfg, Serial);
    const bool may_write = allow_writes && prod::kAutoProvision &&
                           (force_provision || failures < prod::kMaxProvisionFailures);
    if (allow_writes && prod::kAutoProvision && !may_write) {
      Serial.printf("[LoRa] provisionamento suspenso apos %u falhas (console: radio provision)\n",
                    failures);
    }
    if (may_write || force_provision) {
      const BeeCoolerLoraAt::Target target = {
          prod::kTargetBaud, prod::kLevel, prod::kSwitch1 ? 1 : BeeCoolerLoraAt::kUnknown,
          prod::kBaudCodes115200, sizeof(prod::kBaudCodes115200)};
      const BeeCoolerLoraAt::Outcome o = BeeCoolerLoraAt::provision(port, target, cfg, &Serial);
      feed();
      Serial.printf("[LoRa] provisionamento: %s\n", BeeCoolerLoraAt::outcomeName(o));
      const bool failed = o == BeeCoolerLoraAt::Outcome::kFailed ||
                          o == BeeCoolerLoraAt::Outcome::kPartial;
      const uint8_t next = failed ? static_cast<uint8_t>(min<int>(255, failures + 1)) : 0U;
      if (next != failures && gPrefs.begin("bee", false)) {
        gPrefs.putUChar("lprov", next);
        gPrefs.end();
      }
      ok = cfg.valid;
      if (ok && o == BeeCoolerLoraAt::Outcome::kChanged) BeeCoolerLoraAt::printConfig(cfg, Serial);
    }
  }

  const uint32_t baud = ok ? cfg.uart_baud : preferred;
  const bool pins = ok ? cfg.switch_mode == 1 : port.pin_control;
  if (ok && (baud != cached_baud || pins != cached_pins) && gPrefs.begin("bee", false)) {
    gPrefs.putUInt("lbaud", baud);  // so quando muda (desgaste da NVS)
    gPrefs.putBool("lsw1", pins);
    gPrefs.end();
  }

  gState.lora_baud = baud;
  gState.lora_pin_control = pins ? 1U : 0U;
  gState.radio_at_ok = ok ? 1U : 0U;
  gState.lora_level = static_cast<int8_t>(ok ? cfg.level : -1);
  gState.records_per_frame = recordsForLevel(gState.lora_level);
  gState.radio_recheck = 0U;
  refreshAckTimeout();

  if (!ok) Serial.println("[LoRa] ATENCAO: modulo nao respondeu AT; seguindo com a UART da NVS/fabrica");
  if (ok && cfg.level != prod::kLevel) {
    Serial.printf("[LoRa] ATENCAO: LEVEL %d diferente do alvo %d (a central precisa do mesmo)\n",
                  cfg.level, prod::kLevel);
  }
  if (!pins) {
    Serial.println("[LoRa] ATENCAO: SWITCH=0, radio sem sleep no deep sleep (ver BEE_LORA_SWITCH1)");
  }
  beginLoraDriver();
  printRadioProfile();
  return ok;
}

// =============================================================================
// Sessao LoRa (Arquitetura 6.5/6.6)
// =============================================================================

struct SessionResult {
  bool status_acked;
  bool completed;
  uint8_t frames_sent;
  uint32_t acked_up_to;
  uint32_t min_rtt_us;
  uint32_t max_rtt_us;
  uint32_t retransmissions;
  bool clock_adjusted;
};

// Fora da pilha: a sessao inteira usa estes dois buffers.
uint8_t gFrame[kMaxFrameSize];
RecordV1 gFrameRecords[kMaxRecordsPerFrame];
bool gClockAdjusted = false;

void sampleVbatUnderLoad(void*) { gState.vbat_tx_mv = BatteryMonitor::readMv(); }

Header makeHeader(FrameType type, uint32_t first_seq, uint8_t n_rec) {
  Header h = {};
  h.type = type;
  h.node_id = prod::kNodeId;
  h.boot_id = static_cast<uint8_t>(gState.boot_count & 0xFF);
  h.frame_seq = gState.frame_seq;
  h.attempt = 1;
  h.first_seq = first_seq;
  h.n_rec = n_rec;
  return h;
}

uint8_t recordsPerFrame() {
  const uint8_t n = gState.records_per_frame;
  if (n == 0U) return 1U;
  return n > kMaxRecordsPerFrame ? static_cast<uint8_t>(kMaxRecordsPerFrame) : n;
}

// UC-06: hora do ACK so se plausivel; corrige o DS1302 se o desvio passar de 2 s.
void applyEpoch(uint32_t epoch) {
  if (epoch == 0 || !plausibleEpoch(epoch)) return;  // central sem NTP / absurda
  uint32_t local = 0;
  if (readClock(local)) {
    const uint32_t diff = epoch > local ? epoch - local : local - epoch;
    if (diff > kClockMaxJumpS) return;
    if (diff <= kClockToleranceS) {
      gState.last_sync_epoch = epoch;
      return;
    }
  }
  gClock.writeEpoch(epoch);
  gState.last_sync_epoch = epoch;
  gClockAdjusted = true;
}

// Um quadro com ate `max_attempts` tentativas; o cronometro do ACK comeca na
// queda do AUX (fim do TX).
bool exchange(size_t size, uint16_t frame_seq, Ack& ack, SessionResult* stats,
              bool sample_vbat, uint8_t max_attempts, uint32_t timeout_ms, uint32_t* rtt_out) {
  for (uint8_t attempt = 1; attempt <= max_attempts; ++attempt) {
    feed();
    if (setAttempt(gFrame, size, attempt) != Result::kOk) return false;
    gLora.flushInput();
    gParser.reset();
    uint32_t tx_end_us = 0;
    const bool vbat = prod::kVbatCircuit && sample_vbat && attempt == 1;
    if (gLora.send(gFrame, size, tx_end_us, 2000, vbat ? sampleVbatUnderLoad : nullptr, nullptr)) {
      while ((micros() - tx_end_us) < timeout_ms * 1000UL) {
        while (gLora.available() > 0) {
          bool got = gParser.feed(static_cast<uint8_t>(gLora.read()));
          while (got) {
            Frame d;
            if (decodeFrame(gParser.frame(), gParser.frameSize(), d) == Result::kOk &&
                d.is_ack && d.ack.node_id == prod::kNodeId && d.ack.frame_seq == frame_seq) {
              const uint32_t rtt = micros() - tx_end_us;
              ack = d.ack;
              if (rtt_out != nullptr) *rtt_out = rtt;
              if (stats != nullptr) {
                if (stats->min_rtt_us == 0 || rtt < stats->min_rtt_us) stats->min_rtt_us = rtt;
                if (rtt > stats->max_rtt_us) stats->max_rtt_us = rtt;
              }
              return true;
            }
            got = gParser.poll();
          }
        }
        delay(1);
      }
    }
    if (stats != nullptr && attempt > 1) ++stats->retransmissions;
    if (attempt < max_attempts) delay(random(kBackoffMinMs, kBackoffMaxMs + 1));
  }
  return false;
}

bool sendHello(uint8_t reset_reason, bool rtc_valid) {
  gClockAdjusted = false;
  if (!gLora.setMode(BeeCoolerLora::Mode::kNormal)) return false;
  const Hello h = {prod::kFirmwareVersion, reset_reason, static_cast<uint8_t>(rtc_valid ? 1 : 0),
                   gState.last_seq, gState.boot_count};
  size_t size = 0;
  const uint16_t frame_seq = gState.frame_seq;
  encodeHello(makeHeader(FrameType::kHello, gState.last_seq, 0), h, gFrame, sizeof(gFrame), size);
  Ack ack = {};
  const bool ok = exchange(size, frame_seq, ack, nullptr, false,
                           static_cast<uint8_t>(kMaxAttempts), gState.ack_timeout_ms, nullptr);
  ++gState.frame_seq;
  if (ok) {
    gState.confirmed_seq = min(ack.acked_up_to, gState.last_seq);
    gState.last_ack_rssi = ack.rssi;
    applyEpoch(ack.epoch);
  }
  gLora.setMode(BeeCoolerLora::Mode::kSleep);  // sem efeito com SWITCH=0
  return ok;
}

// STATUS -> ACK -> DATA de acked_up_to+1, mais antigo primeiro, <= 36 quadros.
// Um quadro que esgota as tentativas aborta a sessao; os dados ficam no ring.
SessionResult runSession(const RecordV1& latest, bool status_only, uint8_t reset_reason) {
  SessionResult result = {};
  gClockAdjusted = false;
  const uint32_t timeout_ms = gState.ack_timeout_ms;
  const uint8_t per_frame = recordsPerFrame();

  if (!gLora.setMode(BeeCoolerLora::Mode::kNormal)) {
    if (gState.failed_sessions < 255) ++gState.failed_sessions;
    return result;
  }

  {
    Status s = {};
    s.record = latest;
    s.backlog = static_cast<uint16_t>(
        min<uint32_t>(0xFFFF, gState.last_seq - min(gState.confirmed_seq, gState.last_seq)));
    s.sd_free_mb = gState.sd_free_mb;
    s.sd_errors = gState.sd_errors;
    s.reset_reason = reset_reason;
    s.rssi = gState.last_ack_rssi;
    s.failed_sessions = gState.failed_sessions;
    s.vbat_tx_mv = prod::kVbatCircuit ? gState.vbat_tx_mv : kInvalidU16;
    s.fw_version = prod::kFirmwareVersion;
    size_t size = 0;
    const uint16_t frame_seq = gState.frame_seq;
    encodeStatus(makeHeader(FrameType::kStatus, gState.last_seq, 1), s, gFrame, sizeof(gFrame),
                 size);
    Ack ack = {};
    const bool ok = exchange(size, frame_seq, ack, &result, true,
                             static_cast<uint8_t>(kMaxAttempts), timeout_ms, nullptr);
    ++gState.frame_seq;
    if (!ok) {
      if (gState.failed_sessions < 255) ++gState.failed_sessions;
      gLora.setMode(BeeCoolerLora::Mode::kSleep);
      return result;
    }
    result.status_acked = true;
    result.acked_up_to = ack.acked_up_to;
    gState.confirmed_seq = min(ack.acked_up_to, gState.last_seq);
    gState.last_ack_rssi = ack.rssi;
    gState.failed_sessions = 0;
    applyEpoch(ack.epoch);
  }

  bool aborted = false;
  if (!status_only) {
    uint32_t next = max(result.acked_up_to + 1, gRing.oldestSeq(gState.last_seq));
    while (next <= gState.last_seq && result.frames_sent < kMaxDataFramesPerSession) {
      const uint8_t n = static_cast<uint8_t>(min<uint32_t>(per_frame, gState.last_seq - next + 1));
      for (uint8_t i = 0; i < n; ++i) {
        // Slot ilegivel vai como registro vazio para nao travar a sequencia.
        if (!gRing.read(next + i, gFrameRecords[i])) initInvalid(gFrameRecords[i]);
      }
      size_t size = 0;
      const uint16_t frame_seq = gState.frame_seq;
      encodeData(makeHeader(FrameType::kData, next, n), gFrameRecords, n, gFrame, sizeof(gFrame),
                 size);
      Ack ack = {};
      const bool ok = exchange(size, frame_seq, ack, &result, false,
                               static_cast<uint8_t>(kMaxAttempts), timeout_ms, nullptr);
      ++gState.frame_seq;
      if (!ok || ack.acked_up_to < next + n - 1U) {
        aborted = true;
        break;
      }
      ++result.frames_sent;
      result.acked_up_to = ack.acked_up_to;
      gState.confirmed_seq = min(ack.acked_up_to, gState.last_seq);
      next = ack.acked_up_to + 1;
    }
  }
  result.completed = !aborted && (status_only || result.acked_up_to >= gState.last_seq);
  result.clock_adjusted = gClockAdjusted;
  gLora.setMode(BeeCoolerLora::Mode::kSleep);
  return result;
}

// Ensaio do tempo da troca (console `rtt`, relatorio 8.2): quadros de uma
// tentativa. Com registros ja confirmados, reenvia os mais novos em um DATA
// do tamanho maximo (a central so re-confirma: nada e gravado nem enviado a
// nuvem); senao usa HELLO.
void runRtt(long requested) {
  const uint16_t n = requested > 0 ? static_cast<uint16_t>(min<long>(requested, prod::kRttMaxSamples))
                                   : 100U;
  auto* samples = static_cast<uint32_t*>(heap_caps_malloc(n * sizeof(uint32_t), MALLOC_CAP_8BIT));
  if (samples == nullptr) {
    Serial.println("[rtt] sem memoria");
    return;
  }
  if (!gLora.setMode(BeeCoolerLora::Mode::kNormal)) {
    Serial.println("[rtt] AUX ocupado: modulo nao ficou pronto");
    heap_caps_free(samples);
    return;
  }
  const uint8_t per_frame = recordsPerFrame();
  const bool data = gState.confirmed_seq >= per_frame;
  uint16_t acked = 0;
  size_t size = 0;
  for (uint16_t i = 0; i < n; ++i) {
    const uint16_t frame_seq = gState.frame_seq;
    if (data) {
      const uint32_t first = gState.confirmed_seq - per_frame + 1U;
      for (uint8_t r = 0; r < per_frame; ++r) {
        if (!gRing.read(first + r, gFrameRecords[r])) initInvalid(gFrameRecords[r]);
      }
      encodeData(makeHeader(FrameType::kData, first, per_frame), gFrameRecords, per_frame, gFrame,
                 sizeof(gFrame), size);
    } else {
      const Hello h = {prod::kFirmwareVersion, 0U, 0U, gState.last_seq, gState.boot_count};
      encodeHello(makeHeader(FrameType::kHello, gState.last_seq, 0), h, gFrame, sizeof(gFrame),
                  size);
    }
    Ack ack = {};
    uint32_t rtt = 0;
    if (exchange(size, frame_seq, ack, nullptr, false, 1U, prod::kRttSampleTimeoutMs, &rtt)) {
      samples[acked++] = rtt;
    }
    ++gState.frame_seq;
    if ((i + 1U) % 50U == 0U) Serial.printf("[rtt] %u/%u enviados, %u ACKs\n", i + 1U, n, acked);
    delay(random(100, 300));
  }
  gLora.setMode(BeeCoolerLora::Mode::kSleep);

  Serial.printf("[rtt] quadro %s de %u B: %u/%u ACKs\n", data ? "DATA (duplicata)" : "HELLO",
                static_cast<unsigned>(size), acked, n);
  if (acked > 0) {
    const uint32_t p99 = BeeCoolerLinkBudget::sortAndPercentile(samples, acked, 99U);
    const uint32_t suggested = BeeCoolerLinkBudget::ackTimeoutFromP99Us(p99);
    Serial.printf("[rtt] min=%lu p50=%lu p99=%lu max=%lu us | sugerido (p99 + 30%%) %lu ms\n",
                  static_cast<unsigned long>(samples[0]),
                  static_cast<unsigned long>(samples[(acked - 1U) / 2U]),
                  static_cast<unsigned long>(p99), static_cast<unsigned long>(samples[acked - 1U]),
                  static_cast<unsigned long>(suggested));
    if (acked < 100U) {
      Serial.println("[rtt] menos de 100 ACKs: p99 pouco confiavel, nao gravado");
    } else if (setAckOverrideMs(static_cast<uint16_t>(min<uint32_t>(suggested, prod::kAckMaxMs)))) {
      Serial.printf("[rtt] gravado: espera do ACK %u ms (ackms 0 volta ao padrao)\n",
                    gState.ack_timeout_ms);
    }
  } else {
    Serial.println("[rtt] nenhum ACK: conferir central, LEVEL e canal");
  }
  heap_caps_free(samples);
}

// =============================================================================
// Console de manutencao (so na janela apos o cold boot)
// =============================================================================

uint32_t stackFreeBytes() {
  return static_cast<uint32_t>(uxTaskGetStackHighWaterMark(nullptr)) * sizeof(StackType_t);
}

void printTime(uint32_t epoch) {
  int y, mo, d, h, mi, s;
  BeeCoolerLogic::epochToCivil(epoch, y, mo, d, h, mi, s);
  Serial.printf("%lu = %04d-%02d-%02dT%02d:%02d:%02dZ\n", static_cast<unsigned long>(epoch), y, mo,
                d, h, mi, s);
}

// Linha digitada; vazia apos kConsoleIdleMs sem bytes (o console sai).
bool readLine(char* line, size_t capacity) {
  size_t n = 0;
  uint32_t last = millis();
  for (;;) {
    feed();
    while (Serial.available() > 0) {
      last = millis();
      const char c = static_cast<char>(Serial.read());
      if (c == '\n' || c == '\r') {
        if (n == 0) continue;
        line[n] = '\0';
        return true;
      }
      if (n + 1U < capacity) line[n++] = c;
    }
    if (millis() - last > prod::kConsoleIdleMs) return false;
    delay(5);
  }
}

void consoleHelp() {
  Serial.println("Comandos: help | status | time | settime <epoch UTC> | vbat | kcal <mV> | "
                 "vdiv <valor> | dump [n] |\n"
                 "          radio | radio provision | rtt [n<=500] | ackms <ms|0> | exit");
}

void runConsole(uint32_t window_ms) {
  const uint32_t start = millis();
  while (Serial.available() == 0) {
    if (millis() - start > window_ms) return;
    delay(10);
  }
  while (Serial.available() > 0) Serial.read();
  Serial.println("\n[BeeCooler] console de manutencao (producao); sai sozinho apos 5 min parado");
  consoleHelp();

  char line[64];
  for (;;) {
    Serial.print("> ");
    if (!readLine(line, sizeof(line))) {
      Serial.println("\n[console] 5 min sem comandos: retomando o ciclo");
      return;
    }
    Serial.println(line);
    if (strcmp(line, "exit") == 0) return;
    if (strcmp(line, "status") == 0) {
      Serial.printf("fw=%04X node=%u last_seq=%lu confirmed=%lu boot=%u nivel=%u (%s) "
                    "sessoes_falhas=%u sd_erros=%u pilha_livre_min=%lu B\n",
                    prod::kFirmwareVersion, prod::kNodeId,
                    static_cast<unsigned long>(gState.last_seq),
                    static_cast<unsigned long>(gState.confirmed_seq), gState.boot_count,
                    gState.power.level, prod::kPowerPolicy ? "politica ativa" : "fixo",
                    gState.failed_sessions, gState.sd_errors,
                    static_cast<unsigned long>(stackFreeBytes()));
      Serial.printf("bateria: circuito %s kcal=%.4f vdiv=%.3f\n",
                    prod::kVbatCircuit ? "instalado" : "AUSENTE (registro leva 0xFFFF)",
                    BatteryMonitor::gain(), BatteryMonitor::divider());
      printRadioProfile();
    } else if (strcmp(line, "time") == 0) {
      uint32_t e = 0;
      if (gClock.readEpoch(e)) {
        printTime(e);
        if (!plausibleEpoch(e)) Serial.println("ATENCAO: fora da faixa aceita (antes da compilacao ou 10+ anos depois)");
      } else {
        Serial.println("DS1302 sem hora valida (Clock Halt)");
      }
    } else if (strncmp(line, "settime ", 8) == 0) {
      const uint32_t e = static_cast<uint32_t>(strtoul(line + 8, nullptr, 10));
      if (!plausibleEpoch(e)) {
        Serial.println("recusado: antes da compilacao ou mais de 10 anos depois");
      } else {
        gClock.writeEpoch(e);
        delay(1100);
        uint32_t back = 0;
        if (gClock.readEpoch(back) && back > e && back - e <= 3U) {
          Serial.print("ok, contando: ");
          printTime(back);
        } else {
          Serial.println("FALHA: leitura de volta ou contagem do DS1302 nao confirmada");
        }
      }
    } else if (strcmp(line, "vbat") == 0) {
      Serial.printf("vbat = %u mV%s\n", BatteryMonitor::readMv(),
                    prod::kVbatCircuit ? "" : " (circuito ausente: sem significado)");
    } else if (strncmp(line, "kcal ", 5) == 0) {
      const long mv = strtol(line + 5, nullptr, 10);
      if (mv > 2000 && mv < 5000 && BatteryMonitor::calibrateTo(static_cast<uint16_t>(mv))) {
        Serial.printf("K_CAL = %.4f, vbat = %u mV\n", BatteryMonitor::gain(), BatteryMonitor::readMv());
      } else {
        Serial.println("valor invalido (2000..5000 mV)");
      }
    } else if (strncmp(line, "vdiv ", 5) == 0) {
      const float v = strtof(line + 5, nullptr);
      if (v > 1.0f && v < 50.0f) {
        BatteryMonitor::setDivider(v);
        Serial.printf("DIV = %.3f\n", v);
      } else {
        Serial.println("valor invalido");
      }
    } else if (strncmp(line, "dump", 4) == 0) {
      long n = strlen(line) > 5 ? strtol(line + 5, nullptr, 10) : 10;
      if (n < 1 || n > 1000) n = 10;
      const uint32_t last = gState.last_seq;
      const uint32_t first = last > static_cast<uint32_t>(n) ? last - n + 1 : 1;
      for (uint32_t seq = first; seq <= last && last != 0; ++seq) {
        RecordV1 r;
        if (gRing.read(seq, r)) {
          Serial.printf("seq=%lu ts=%lu t_in=%d rh_in=%u t_out=%d rh_out=%u vbat=%u rms=%u "
                        "peak=%u bands=%02X,%02X,%02X,%02X flags=0x%04X\n",
                        static_cast<unsigned long>(seq), static_cast<unsigned long>(r.ts), r.t_in,
                        r.rh_in, r.t_out, r.rh_out, r.vbat_mv, r.vib_rms, r.vib_peak, r.band_db[0],
                        r.band_db[1], r.band_db[2], r.band_db[3], r.flags);
        } else {
          Serial.printf("seq=%lu slot invalido\n", static_cast<unsigned long>(seq));
        }
        feed();
      }
    } else if (strcmp(line, "radio") == 0) {
      BeeCoolerLoraAt::Port port = loraPort(gState.lora_pin_control != 0U);
      BeeCoolerLoraAt::applyPinMode(port);
      BeeCoolerLoraAt::Config cfg;
      BeeCoolerLoraAt::detect(port, gState.lora_baud, cfg, &Serial);
      BeeCoolerLoraAt::printConfig(cfg, Serial);
      beginLoraDriver();
      printRadioProfile();
    } else if (strcmp(line, "radio provision") == 0) {
      radioBringUp(true, true);
    } else if (strcmp(line, "rtt") == 0 || strncmp(line, "rtt ", 4) == 0) {
      runRtt(strlen(line) > 4 ? strtol(line + 4, nullptr, 10) : 0);
    } else if (strncmp(line, "ackms ", 6) == 0) {
      const long ms = strtol(line + 6, nullptr, 10);
      if (ms >= 0 && ms <= 65535 && setAckOverrideMs(static_cast<uint16_t>(ms))) {
        printRadioProfile();
      } else {
        Serial.printf("valor invalido (0 = automatico, ou %u..%u ms)\n", prod::kAckMinMs,
                      prod::kAckMaxMs);
      }
    } else {
      consoleHelp();
    }
  }
}

// =============================================================================
// Bateria e energia
// =============================================================================

uint16_t restingVbat() {
  if (!prod::kVbatCircuit) return kInvalidU16;  // [R Tabela 12]
  const uint16_t now = BatteryMonitor::readMv();
  auto& h = gState.vbat_history;
  if (gState.vbat_history_count < 3) {
    h[gState.vbat_history_count++] = now;
  } else {
    h[0] = h[1];
    h[1] = h[2];
    h[2] = now;
  }
  return now;
}

bool updatePowerLevel(uint16_t vbat_mv) {
  if (!prod::kPowerPolicy || vbat_mv == kInvalidU16) {
    const bool changed = gState.power.level != 0;
    gState.power = {0, 0, 0};
    return changed;
  }
  const auto& h = gState.vbat_history;
  const uint16_t filtered = gState.vbat_history_count < 3
                                ? h[gState.vbat_history_count - 1]
                                : BeeCoolerLogic::median3(h[0], h[1], h[2]);
  return BeeCoolerLogic::powerUpdate(gState.power, filtered);
}

// =============================================================================
// Sono
// =============================================================================

void lightSleepMs(uint64_t ms) {
  if (ms == 0) return;
  Serial.flush();
  feed();
  esp_sleep_enable_timer_wakeup(ms * 1000ULL);
  esp_light_sleep_start();
  feed();
}

[[noreturn]] void deepSleepFor(uint32_t seconds) {
  if (gState.lora_pin_control) {
    // SWITCH=1: modulo em sleep (M0=M1=1) durante o deep sleep. So GPIO RTC
    // (0-21) seguram nivel; por isso M0/M1 estao em 1 e 2.
    gpio_set_level(static_cast<gpio_num_t>(kPinLoraM0), 1);
    gpio_set_level(static_cast<gpio_num_t>(kPinLoraM1), 1);
    gpio_hold_en(static_cast<gpio_num_t>(kPinLoraM0));
    gpio_hold_en(static_cast<gpio_num_t>(kPinLoraM1));
    gpio_deep_sleep_hold_en();
  }
  // SWITCH=0: M0/M1 sao saidas do modulo; o ESP nao os toca.
  Serial.printf("[sleep] %lu s\n", static_cast<unsigned long>(seconds));
  Serial.flush();
  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(seconds) * 1000000ULL);
  esp_deep_sleep_start();
  for (;;) {}
}

[[noreturn]] void sleepUntilNextSlot(const LevelParams& params, bool rtc_valid,
                                     uint32_t cycle_start_ms) {
  uint32_t now = 0;
  uint32_t seconds;
  if (rtc_valid && readClock(now)) {
    seconds = BeeCoolerLogic::sleepSeconds(now, params.cycle_period_s, kBootMarginS);
  } else {
    const uint32_t elapsed = (millis() - cycle_start_ms) / 1000U;
    seconds = params.cycle_period_s > elapsed + 1 ? params.cycle_period_s - elapsed : 1;
  }
  gBoot.abnormal_resets = 0;  // ciclo completo: zera o rele de reinicio
  deepSleepFor(seconds);
}

// =============================================================================
// Um ciclo
// =============================================================================

struct VibCapture {
  int16_t* buffer = nullptr;
  AdxlWindow window = {};
  BeeCoolerVib::Features features = {};
  uint32_t processing_ms = 0;
};

int16_t* allocateWindowBuffer(size_t samples) {
  const size_t bytes = samples * 3U * sizeof(int16_t);
  void* p = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (p == nullptr) p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
  return static_cast<int16_t*>(p);
}

void runCycle() {
  const uint32_t cycle_start_ms = millis();
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);  // sem PC na USB-CDC, nao bloquear escrevendo
#endif
  watchdogStart();

  const esp_reset_reason_t reason = esp_reset_reason();
  const uint8_t reset_reason = static_cast<uint8_t>(reason);
  if (gBoot.magic != kBootGuardMagic || reason == ESP_RST_POWERON) {
    gBoot = {kBootGuardMagic, 0, 0, 0};  // energia nova: comeca do zero
  }
  if (abnormalReset(reason) && gBoot.abnormal_resets < 255) ++gBoot.abnormal_resets;
  gBoot.last_reason = reset_reason;
  const bool after_brownout = reason == ESP_RST_BROWNOUT;

  // Solta o hold que manteve o LoRa em sleep durante o deep sleep.
  gpio_hold_dis(static_cast<gpio_num_t>(kPinLoraM0));
  gpio_hold_dis(static_cast<gpio_num_t>(kPinLoraM1));
  gpio_deep_sleep_hold_dis();

  const bool warm = gState.magic == kNodeStateMagic;  // false: cold boot
  if (!warm) {
    memset(&gState, 0, sizeof(gState));
    gState.magic = kNodeStateMagic;
  }

  // Rele de reinicio: 3 resets anormais seguidos -> um ciclo inteiro dormindo,
  // sem radio. O estado na RTC e refeito no proximo boot (cold boot).
  if (gBoot.abnormal_resets >= prod::kSafeModeAfterResets) {
    Serial.printf("[seguro] %u reinicios anormais seguidos (ultimo=%u): dormindo um ciclo sem radio\n",
                  gBoot.abnormal_resets, reset_reason);
    gBoot.abnormal_resets = prod::kSafeModeAfterResets - 1U;  // mais uma falha volta aqui
    gState.magic = 0;
    gState.lora_pin_control = 0;  // nao dirige M0/M1 sem saber o SWITCH
    deepSleepFor(600U / kTimeDivisor);
  }

  gClock.begin(kPinRtcCe, kPinRtcIo, kPinRtcSclk);
  const bool ring_ok = gRing.begin(kRingPartitionLabel, kRingPartitionSubtype);
  BatteryMonitor::begin(kPinVbat);  // so configura o ADC; le apenas com o circuito
  if (kPinCharging >= 0) pinMode(kPinCharging, INPUT_PULLUP);
  // CS do ADXL e do SD em alto antes de qualquer transacao no SPI.
  pinMode(kPinAdxlCs, OUTPUT);
  digitalWrite(kPinAdxlCs, HIGH);
  pinMode(kPinSdCs, OUTPUT);
  digitalWrite(kPinSdCs, HIGH);

  if (!warm) {
    recoverAfterColdBoot(ring_ok);
    Serial.printf("[boot] cold boot #%u reset=%u last_seq=%lu ring=%s fw=%04X node=%u\n",
                  gState.boot_count, reset_reason, static_cast<unsigned long>(gState.last_seq),
                  ring_ok ? "ok" : "FALHA", prod::kFirmwareVersion, prod::kNodeId);
    if (after_brownout) Serial.println("[boot] brownout: nada sera gravado no modulo e sem HELLO neste boot");
    radioBringUp(!after_brownout, false);
    runConsole(kMaintenanceWindowMs);
    feed();
  } else {
    beginLoraDriver();
  }

  uint32_t now = 0;
  bool rtc_valid = readClock(now);

  // HELLO no cold boot: o ACK corrige o relogio (UC-06, UC-10). Pulado depois
  // de um brownout: o pico de 188 mA do TX pode ter sido a causa.
  if (!warm && !after_brownout) {
    const bool acked = sendHello(reset_reason, rtc_valid);
    Serial.printf("[hello] %s\n", acked ? "ACK" : "sem ACK");
    if (gClockAdjusted) gState.pending_flags |= kClockAdjusted;
    rtc_valid = readClock(now);
    saveDsState();
  }

  // ---- Bateria e nivel de energia (antes de qualquer TX)
  const uint16_t vbat_mv = restingVbat();
  if (updatePowerLevel(vbat_mv)) {
    Serial.printf("[power] nivel %u (vbat=%u mV)\n", gState.power.level, vbat_mv);
  }
  const LevelParams params = BeeCoolerLogic::levelParams(gState.power.level, kTimeDivisor);

  const uint32_t slot = rtc_valid ? BeeCoolerLogic::nearestSlot(now, params.cycle_period_s) : 0;
  RecordV1 record;
  initInvalid(record);
  uint16_t flags = gState.pending_flags;
  gState.pending_flags = 0;
  if (!rtc_valid) flags |= kRtcInvalid;
  record.ts = slot;
  record.vbat_mv = vbat_mv;
  if (kPinCharging >= 0 && digitalRead(kPinCharging) == LOW) flags |= kCharging;

  ExtendedRecord ext;
  memset(&ext, 0, sizeof(ext));
  for (size_t i = 0; i < 12; ++i) ext.sht_raw[i] = kInvalidU16;

  // ---- SHT30 (D52: um sensor ausente nao bloqueia)
  if (params.measure_sht) {
    gSht.begin(kPinShtIntSda, kPinShtIntScl, kPinShtExtSda, kPinShtExtScl);
    const ShtResult sht = gSht.measure();
    gSht.end();
    record.t_in = sht.t_in;
    record.rh_in = sht.rh_in;
    record.t_out = sht.t_out;
    record.rh_out = sht.rh_out;
    if (!sht.in_ok) flags |= kShtInFail;
    if (!sht.out_ok) flags |= kShtOutFail;
    for (uint8_t s = 0; s < 2; ++s) {
      for (uint8_t k = 0; k < 3; ++k) {
        ext.sht_raw[s * 6 + k * 2] = sht.raw[s][k][0];
        ext.sht_raw[s * 6 + k * 2 + 1] = sht.raw[s][k][1];
      }
    }
    BeeCoolerLogic::stuckPush(gState.stuck, sht.t_in, sht.rh_in, sht.t_out);
    if (BeeCoolerLogic::stuckDetected(gState.stuck)) flags |= kShtInStuck;
  }
  feed();

  // ---- Janela de vibracao (96 kB no heap; FFT estatica na BeeCoolerVib)
  VibCapture vib;
  sharedSpiBegin();
  gAdxl.begin(kPinAdxlCs, kPinAdxlInt1, kPinAdxlInt2);
  if (params.measure_vibration) {
    vib.buffer = allocateWindowBuffer(kAdxlWindowSamples);
    if (vib.buffer == nullptr) {
      flags |= kVibSkipped;
      gAdxl.standby();
    } else {
      vib.window = gAdxl.capture(vib.buffer, kAdxlWindowSamples, kAdxlWindowTimeoutMs,
                                 kMechEventEnabled);
      if (!vib.window.device_ok) {
        flags |= kAdxlFail | kVibSkipped;
      } else {
        if (!vib.window.complete) flags |= kAdxlFail;
        if (vib.window.overruns > 0) flags |= kFifoOverrun;
        if (vib.window.activity) flags |= kMechEvent;
        if (vib.window.samples >= BeeCoolerVib::kSegment) {
          const uint32_t t0 = millis();
          vib.features = BeeCoolerVib::compute(vib.buffer, vib.window.samples,
                                               static_cast<float>(kAdxlOdrHz));
          vib.processing_ms = millis() - t0;
        }
        if (vib.features.valid) {
          record.vib_rms = BeeCoolerVib::toRecordMg10(vib.features.rms_mg);
          record.vib_peak = BeeCoolerVib::toRecordMg10(vib.features.peak_mg);
          for (size_t b = 0; b < BeeCoolerVib::kBands; ++b) {
            record.band_db[b] = encodeBandDb(vib.features.band_db[b]);
          }
        } else {
          flags |= kVibSkipped;
        }
      }
    }
  } else {
    flags |= kVibSkipped;
    gAdxl.standby();
  }
  feed();

  flags = setPowerLevel(flags, gState.power.level);
  const uint32_t seq = gState.last_seq + 1;

  // ---- microSD: RAW + registro estendido (falha nao derruba nada; D50)
  SdRawSink sd;
  NullRawSink null_sink;
  RawSink* sink = &null_sink;
  if (kSdEnabled) {
    if (sd.begin(kPinSdCs, prod::kNodeId, slot)) {
      sink = &sd;
      if (gState.cycles_since_sd_refresh == 0 ||
          gState.cycles_since_sd_refresh >= kSdFreeSpaceRefreshCycles) {
        gState.sd_free_mb = sd.freeMegabytes();
        gState.cycles_since_sd_refresh = 0;
      }
      ++gState.cycles_since_sd_refresh;
    } else {
      flags |= kSdFail;
      if (gState.sd_errors < 255) ++gState.sd_errors;
    }
  }
  if (sink->available()) {
    const uint32_t t0 = millis();
    bool ok = true;
    if (vib.buffer != nullptr && vib.window.samples > 0) {
      RawHeader header;
      memset(&header, 0, sizeof(header));
      memcpy(header.magic, "BEEV", 4);
      header.version = 1;
      header.node_id = prod::kNodeId;
      header.odr_code = kAdxlOdrCode;
      header.range_code = kAdxlRangeCode;
      header.seq = seq;
      header.ts_start = slot;
      header.n_samples = static_cast<uint16_t>(vib.window.samples);
      header.fifo_overruns = vib.window.overruns;
      header.t_in = record.t_in;
      header.flags = flags;
      ok = sink->writeWindow(header, vib.buffer, vib.window.samples);
    }
    ext.seq = seq;
    ext.record = record;
    ext.record.flags = flags;
    ext.wake_ms = static_cast<uint16_t>(min<uint32_t>(0xFFFF, millis() - cycle_start_ms));
    ext.processing_ms = static_cast<uint16_t>(min<uint32_t>(0xFFFF, vib.processing_ms));
    ext.fifo_overruns = static_cast<uint8_t>(min<uint16_t>(255, vib.window.overruns));
    ext.vbat_tx_mv = prod::kVbatCircuit ? gState.vbat_tx_mv : kInvalidU16;
    ext.sd_write_ms = static_cast<uint16_t>(min<uint32_t>(0xFFFF, millis() - t0));
    finalizeExtended(ext);
    ok = sink->writeExtended(ext) && ok;
    if (!ok) {
      flags |= kSdFail;
      if (gState.sd_errors < 255) ++gState.sd_errors;
    }
  }
  if (vib.buffer != nullptr) {
    heap_caps_free(vib.buffer);
    vib.buffer = nullptr;
  }
  feed();

  // ---- Persistir antes de qualquer transmissao (RF-04)
  record.flags = flags;
  bool stored = false;
  if (ring_ok && gRing.append(seq, record)) {
    gState.last_seq = seq;
    stored = true;
    saveDsState();
  } else {
    Serial.println("[ring] FALHA ao gravar o registro");
    if (sink->available()) sink->logEvent("ring append failed");
  }
  Serial.printf("[rec] seq=%lu slot=%lu t_in=%d rh_in=%u t_out=%d rh_out=%u vbat=%u rms=%u "
                "peak=%u flags=0x%04X %s\n",
                static_cast<unsigned long>(seq), static_cast<unsigned long>(slot), record.t_in,
                record.rh_in, record.t_out, record.rh_out, record.vbat_mv, record.vib_rms,
                record.vib_peak, record.flags, stored ? "" : "(NAO GRAVADO)");

  // ---- Sessao LoRa no slot do no
  bool run_session = false;
  if (params.radio) {
    if (rtc_valid) {
      run_session = BeeCoolerLogic::isLoraSlot(slot, params.lora_period_s) &&
                    gState.last_lora_slot != slot;
    } else {
      const uint32_t every = max<uint32_t>(1, params.lora_period_s / params.cycle_period_s);
      run_session = ++gState.cycles_since_lora >= every;
    }
  }
  if (run_session) {
    if (gState.radio_recheck) {
      Serial.println("[lora] conferindo o modulo por AT apos sessoes sem ACK");
      radioBringUp(!after_brownout, false);
    }
    if (rtc_valid) {
      // t_tx = slot + 20 s + (node_id - 1) * 30 s + jitter (0-2 s)
      const uint32_t offset = BeeCoolerLogic::loraOffsetSeconds(prod::kNodeId);
      const uint32_t target = slot + offset;
      uint32_t t = 0;
      if (readClock(t) && target > t && target - t <= offset + 5U) {
        lightSleepMs(static_cast<uint64_t>(target - t) * 1000ULL +
                     static_cast<uint64_t>(random(0, kSlotJitterMaxMs + 1)));
      }
    }
    const SessionResult r = runSession(record, params.status_only, reset_reason);
    gState.last_lora_slot = slot;
    gState.cycles_since_lora = 0;
    if (r.clock_adjusted) gState.pending_flags |= kClockAdjusted;
    if (!r.status_acked && gState.failed_sessions >= prod::kRecheckAfterFailedSessions &&
        gState.failed_sessions % prod::kRecheckAfterFailedSessions == 0) {
      gState.radio_recheck = 1;
    }
    saveDsState();

    char line[160];
    snprintf(line, sizeof(line),
             "lora status=%d done=%d frames=%u acked=%lu retx=%lu rtt_us=%lu..%lu "
             "falhas=%u espera=%ums",
             r.status_acked, r.completed, r.frames_sent, static_cast<unsigned long>(r.acked_up_to),
             static_cast<unsigned long>(r.retransmissions),
             static_cast<unsigned long>(r.min_rtt_us), static_cast<unsigned long>(r.max_rtt_us),
             gState.failed_sessions, gState.ack_timeout_ms);
    Serial.printf("[%s]\n", line);
    if (sink->available()) sink->logEvent(line);
    if (r.clock_adjusted) rtc_valid = readClock(now) || rtc_valid;
  }

#ifdef BEE_TEST_FAST
  if (sink->available()) sink->logEvent("TEST_FAST: registros fora do dataset");
#endif
  sink->end();

  Serial.printf("[cycle] acordado por %lu ms | [stack] livre minima %lu B | heap livre %lu B\n",
                static_cast<unsigned long>(millis() - cycle_start_ms),
                static_cast<unsigned long>(stackFreeBytes()),
                static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
  sleepUntilNextSlot(params, rtc_valid, cycle_start_ms);
}

}  // namespace

void setup() { runCycle(); }

void loop() {}
