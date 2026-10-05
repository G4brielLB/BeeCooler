// BeeCooler - teste de comunicacao, no sensor (ESP32-S3).
//
// Diagnostico de bancada: sem deep sleep ou ring. Testa escrita/leitura do
// microSD. A cada 60 s le os sensores (SHT30, ADXL345, bateria, DS1302) e
// envia um quadro DATA por LoRa (protocolo real BeeCoolerLink, com ACK e ate
// 4 tentativas). Tudo que acontece sai no monitor serial.

#include <Arduino.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include <esp_random.h>

#include <BeeCoolerLink.h>
#include <BeeCoolerBenchConfig.h>
#include <BeeCoolerBenchRadio.h>
#include <BeeCoolerLogic.h>
#include <BeeCoolerLora.h>
#include <BeeCoolerRecord.h>
#include <BeeCoolerVib.h>

#include "../sensor/Adxl345Fifo.h"
#include "../sensor/BatteryMonitor.h"
#include "../sensor/Ds1302Clock.h"
#include "../sensor/SharedSpi.h"
#include "../sensor/Sht30Pair.h"
#include "../sensor/config.h"
#include "../sensor/pins.h"

using namespace BeeCoolerRecord;
using namespace BeeCoolerLink;

namespace {

#ifndef BEE_TEST_CYCLE_PERIOD_MS
#define BEE_TEST_CYCLE_PERIOD_MS 60000U
#endif
#ifndef BEE_TEST_ACK_TIMEOUT_MS
#define BEE_TEST_ACK_TIMEOUT_MS 2000U
#endif
constexpr uint32_t kCyclePeriodMs = BEE_TEST_CYCLE_PERIOD_MS;
constexpr uint32_t kTestAckTimeoutMs = BEE_TEST_ACK_TIMEOUT_MS;
static_assert(kCyclePeriodMs > 0 && kCyclePeriodMs <= INT32_MAX,
              "Invalid bench cycle period");
static_assert(kTestAckTimeoutMs > 0 && kTestAckTimeoutMs <= 60000U,
              "Invalid bench ACK timeout");
constexpr uint8_t kNodeId = BEE_NODE_ID;

Ds1302Clock gClock;
BeeCoolerLora gLora;
Sht30Pair gSht;
Adxl345Fifo gAdxl;
StreamParser gParser;
int16_t* gWindow = nullptr;

uint8_t gBootId = 0;
uint16_t gFrameSeq = 1;
uint32_t gSeq = 0;
uint32_t gBuildEpoch = 0;
uint32_t gNextCycleMs = 0;
bool gRadioInitialized = false;
bool gSdMounted = false;
uint32_t gAckedCycles = 0;

bool readClock(uint32_t& epoch) {
  // A valid retained calendar can predate this firmware's compilation.
  return gClock.readEpoch(epoch);
}

void printDateTime(uint32_t epoch) {
  int year, month, day, hour, minute, second;
  BeeCoolerLogic::epochToCivil(epoch, year, month, day, hour, minute, second);
  Serial.printf("%04d-%02d-%02d %02d:%02d:%02d", year, month, day,
                hour, minute, second);
}

void printClockRegisters() {
  uint8_t r[8];
  gClock.readClockRegisters(r);
  Serial.printf("[RTC RAW] seg=%02X min=%02X hora=%02X dia=%02X mes=%02X semana=%02X ano=%02X controle=%02X | CH=%u modo12h=%u WP=%u\n",
                r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7],
                (r[0] >> 7) & 1U, (r[2] >> 7) & 1U, (r[7] >> 7) & 1U);
  Serial.printf("[RTC] conferir RST/CE->GPIO%d DAT/IO->GPIO%d CLK->GPIO%d, VCC e GND comum\n",
                kPinRtcCe, kPinRtcIo, kPinRtcSclk);
}

bool testClock(uint32_t& epoch) {
  uint32_t before = 0;
  if (!readClock(before)) {
    Serial.println("[RTC] FALHA: calendario invalido, parado ou sem resposta");
    printClockRegisters();
    return false;
  }
  delay(1100);  // Readback alone cannot detect a stopped oscillator.
  if (!readClock(epoch) || epoch <= before || epoch - before > 3U) {
    Serial.println("[RTC] FALHA: segundos nao avancaram normalmente");
    printClockRegisters();
    return false;
  }
  Serial.print("[RTC] CONTANDO; DATA/HORA NAO VALIDADA: ");
  printDateTime(epoch);
  Serial.printf(" | epoch=%lu | segundos avancando\n",
                static_cast<unsigned long>(epoch));
  Serial.println("      Segundos avancando nao comprovam data/hora correta; conferir com seu relogio.");
  return true;
}

void printSht(const char* label, bool ok, int16_t temperature, uint16_t humidity) {
  if (ok) {
    Serial.printf("[%s] OK: %.2f C | %.2f %%RH\n", label,
                  temperature / 100.0f, humidity / 100.0f);
  } else {
    Serial.printf("[%s] FALHA: nenhuma leitura valida com CRC\n", label);
  }
}

void printShtDiagnostics(const char* label, const ShtDiagnostics& d) {
  Serial.printf("[%s I2C] inicio=%s presente=%s endereco=0x%02X | probe44=%u probe45=%u (0=ACK, 255=nao testado)\n",
                label, d.bus_started ? "OK" : "FALHA",
                d.address_found ? "SIM" : "NAO", d.address,
                d.probe_status[0], d.probe_status[1]);
  Serial.printf("[%s I2C] leituras_validas=%u/3 falhas_comando=%u falhas_leitura=%u falhas_validacao=%u\n",
                label, d.valid_reads, d.command_failures, d.read_failures,
                d.decode_failures);
}

bool testSd(uint32_t seq, const RecordV1& record) {
  // Mount after acquisition and retain the mount while awake. Files are
  // still closed/reopened and verified every cycle. Avoid resetting the
  // physical card on every successful diagnostic via SD.end().
  if (!gSdMounted && !SD.begin(kPinSdCs, sharedSpi(), kSdSpiHz)) {
    Serial.println("[SD] FALHA: nao montou; conferir cartao, formato e ligacoes");
    SD.end();
    return false;
  }
  if (SD.cardType() == CARD_NONE) {
    Serial.println("[SD] FALHA: cartao nao detectado");
    SD.end();
    gSdMounted = false;
    return false;
  }
  gSdMounted = true;
  Serial.printf("[SD] detectado: tipo=%u capacidade=%llu MB\n",
                static_cast<unsigned>(SD.cardType()),
                static_cast<unsigned long long>(SD.cardSize() / (1024ULL * 1024ULL)));

  // Never truncate an existing file. Remove only this cycle's test file.
  char path[48];
  snprintf(path, sizeof(path), "/bee_test_%08lx_%08lx.tmp",
           static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(seq));
  if (SD.exists(path)) {
    Serial.println("[SD] FALHA: nome de teste ja existe; arquivo preservado");
    return false;
  }
  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("[SD] FALHA: nao criou arquivo de teste");
    SD.end();
    gSdMounted = false;
    return false;
  }
  const size_t written = file.write(reinterpret_cast<const uint8_t*>(&record),
                                    sizeof(record));
  file.flush();
  file.close();

  RecordV1 readback;
  file = SD.open(path, FILE_READ);
  const bool read_ok = file && file.size() == sizeof(record) &&
      file.read(reinterpret_cast<uint8_t*>(&readback), sizeof(readback)) == sizeof(readback);
  if (file) file.close();
  const bool verified = written == sizeof(record) && read_ok &&
      memcmp(&record, &readback, sizeof(record)) == 0;
  const bool removed = SD.remove(path);
  if (!removed) Serial.printf("[SD] FALHA: nao removeu arquivo de teste %s\n", path);
  Serial.printf("[SD] %s: escrita + fechamento + reabertura + leitura + comparacao (%u bytes)\n",
                verified ? "OK" : "FALHA", static_cast<unsigned>(sizeof(record)));
  if (!verified || !removed) {
    SD.end();
    gSdMounted = false;  // Try a fresh mount next cycle after an I/O failure.
  }
  return verified && removed;
}

// Uma tentativa por vez, ate kMaxAttempts. O cronometro do ACK comeca quando
// o AUX cai (fim do TX), como em LoraSession::exchange.
bool sendWithAck(uint8_t* frame, size_t size, uint16_t frame_seq, uint32_t seq,
                 uint8_t& attempts, uint32_t& rtt_us) {
  for (uint8_t attempt = 1; attempt <= kMaxAttempts; ++attempt) {
    attempts = attempt;
    Serial.printf("[LoRa] tentativa %u/%u: quadro=%u registro=%lu\n", attempt,
                  static_cast<unsigned>(kMaxAttempts), frame_seq,
                  static_cast<unsigned long>(seq));
    if (setAttempt(frame, size, attempt) != Result::kOk) return false;

    gLora.flushInput();
    gParser.reset();
    uint32_t tx_end_us = 0;
    if (gLora.send(frame, size, tx_end_us)) {
      Serial.println("[LoRa] TX local concluido; aguardando ACK do gateway...");
      while ((micros() - tx_end_us) < kTestAckTimeoutMs * 1000UL) {
        while (gLora.available() > 0) {
          bool got = gParser.feed(static_cast<uint8_t>(gLora.read()));
          while (got) {
            Frame decoded;
            if (decodeFrame(gParser.frame(), gParser.frameSize(), decoded) ==
                    Result::kOk &&
                decoded.is_ack && decoded.ack.node_id == kNodeId &&
                decoded.ack.frame_seq == frame_seq && decoded.ack.acked_up_to >= seq) {
              rtt_us = micros() - tx_end_us;
              return true;
            }
            got = gParser.poll();
          }
        }
        delay(1);
      }
      Serial.println("[LoRa] timeout: nenhum ACK correspondente ao registro");
    } else {
      Serial.println("[LoRa] FALHA TX local: verificar UART, AUX, modulo e alimentacao");
    }
    if (attempt < kMaxAttempts) {
      delay(random(kBackoffMinMs, kBackoffMaxMs + 1));
    }
  }
  return false;
}

void runCycle() {
  const uint32_t cycle_start_ms = millis();
  const uint32_t seq = ++gSeq;
  Serial.printf("\n========== DIAGNOSTICO %lu ==========\n",
                static_cast<unsigned long>(seq));

  RecordV1 rec;
  initInvalid(rec);
  uint16_t flags = 0;

  // ---- DS1302
  uint32_t now = 0;
  const bool rtc_ok = testClock(now);
  if (rtc_ok) {
    rec.ts = now;
  } else {
    flags |= kRtcInvalid;
    rec.ts = cycle_start_ms / 1000U;  // segundos desde o boot
  }

  // ---- Bateria
  rec.vbat_mv = BatteryMonitor::readMv();
  Serial.printf("[BATERIA] leitura=%.3f V | divisor=%.3f | calibracao=%.4f\n",
                rec.vbat_mv / 1000.0f, BatteryMonitor::divider(), BatteryMonitor::gain());
  Serial.println("          Conferir com multimetro; ADC sozinho nao confirma sensor/calibracao.");
  if (kPinCharging >= 0 && digitalRead(kPinCharging) == LOW) flags |= kCharging;
  if (kPinCharging >= 0) {
    Serial.printf("[CHRG] GPIO%d=%s (LOW indica carga; HIGH pode ser desconectado)\n",
                  kPinCharging, (flags & kCharging) ? "LOW" : "HIGH");
  }

  // ---- SHT30
  gSht.begin(kPinShtIntSda, kPinShtIntScl, kPinShtExtSda, kPinShtExtScl);
  const ShtResult sht = gSht.measure();
  gSht.end();
  rec.t_in = sht.t_in;
  rec.rh_in = sht.rh_in;
  rec.t_out = sht.t_out;
  rec.rh_out = sht.rh_out;
  if (!sht.in_ok) flags |= kShtInFail;
  if (!sht.out_ok) flags |= kShtOutFail;
  printSht("SHT30 INTERNO", sht.in_ok, sht.t_in, sht.rh_in);
  printSht("SHT30 EXTERNO", sht.out_ok, sht.t_out, sht.rh_out);
  if (!sht.in_ok) printShtDiagnostics("SHT30 INTERNO", gSht.diagnostics(false));
  if (!sht.out_ok) printShtDiagnostics("SHT30 EXTERNO", gSht.diagnostics(true));

  // ---- ADXL345 (janela de 10 s)
  bool adxl_ok = false;
  if (gWindow == nullptr) {
    Serial.println("[ADXL345] FALHA: sem memoria para captura");
    flags |= kVibSkipped;
  } else {
    Serial.println("[adxl] capturando janela de 10 s...");
    Serial.flush();
    const AdxlWindow w = gAdxl.capture(gWindow, kAdxlWindowSamples,
                                       kAdxlWindowTimeoutMs, false);
    if (!w.device_ok) {
      Serial.println("[ADXL345] FALHA: DEVID diferente de 0xE5 (SPI/ligacoes)");
      flags |= kAdxlFail | kVibSkipped;
    } else {
      Serial.printf("[ADXL345] DEVID=0xE5 | amostras=%lu/%lu por eixo | duracao=%lu ms | overruns=%u\n",
                    static_cast<unsigned long>(w.samples),
                    static_cast<unsigned long>(kAdxlWindowSamples),
                    static_cast<unsigned long>(w.duration_ms), w.overruns);
      if (!w.complete) {
        flags |= kAdxlFail;
        Serial.println("[ADXL345] FALHA: janela incompleta (timeout)");
      }
      if (w.overruns > 0) Serial.println("[ADXL345] ATENCAO: FIFO perdeu amostras");
      if (w.samples > 0) {
        Serial.printf("[ADXL345] primeira amostra: X=%d Y=%d Z=%d LSB | %.4f %.4f %.4f g\n",
                      gWindow[0], gWindow[1], gWindow[2],
                      gWindow[0] * BeeCoolerVib::kMgPerLsb / 1000.0f,
                      gWindow[1] * BeeCoolerVib::kMgPerLsb / 1000.0f,
                      gWindow[2] * BeeCoolerVib::kMgPerLsb / 1000.0f);
      }
      if (w.overruns > 0) flags |= kFifoOverrun;
      BeeCoolerVib::Features f = {};
      if (w.samples >= BeeCoolerVib::kSegment) {
        f = BeeCoolerVib::compute(gWindow, w.samples,
                                  static_cast<float>(kAdxlOdrHz));
      }
      if (f.valid) {
        adxl_ok = w.complete && w.overruns == 0;
        Serial.printf("[VIBRACAO] RMS=%.2f mg | pico=%.2f mg | segmentos=%u\n",
                      f.rms_mg, f.peak_mg, static_cast<unsigned>(f.segments));
        for (size_t b = 0; b < BeeCoolerVib::kBands; ++b) {
          Serial.printf("           %.0f-%.0f Hz: %.2f dB\n",
                        BeeCoolerVib::kBandEdgesHz[b], BeeCoolerVib::kBandEdgesHz[b + 1],
                        f.band_db[b]);
        }
        rec.vib_rms = BeeCoolerVib::toRecordMg10(f.rms_mg);
        rec.vib_peak = BeeCoolerVib::toRecordMg10(f.peak_mg);
        for (size_t b = 0; b < BeeCoolerVib::kBands; ++b) {
          rec.band_db[b] = encodeBandDb(f.band_db[b]);
        }
      } else {
        Serial.println("[VIBRACAO] FALHA: amostras insuficientes para calcular features");
        flags |= kVibSkipped;
      }
    }
  }
  rec.flags = flags;
  const bool sd_ok = testSd(seq, rec);
  if (!sd_ok) rec.flags |= kSdFail;
  Serial.printf("[rec] seq=%lu ts=%lu t_in=%d rh_in=%u t_out=%d rh_out=%u "
                "vbat=%u rms=%u peak=%u bands=%u,%u,%u,%u flags=0x%04X\n",
                static_cast<unsigned long>(seq),
                static_cast<unsigned long>(rec.ts), rec.t_in, rec.rh_in,
                rec.t_out, rec.rh_out, rec.vbat_mv, rec.vib_rms, rec.vib_peak,
                rec.band_db[0], rec.band_db[1], rec.band_db[2], rec.band_db[3],
                rec.flags);

  // ---- LoRa: um quadro DATA com este registro
  Header h = {};
  h.type = FrameType::kData;
  h.node_id = kNodeId;
  h.boot_id = gBootId;
  h.frame_seq = gFrameSeq;
  h.attempt = 1;
  h.first_seq = seq;
  h.n_rec = 1;

  uint8_t frame[kMaxFrameSize];
  size_t size = 0;
  bool lora_ok = false;
  const bool radio_ready = gRadioInitialized && gLora.setMode(BeeCoolerLora::Mode::kNormal);
  if (!radio_ready) {
    Serial.println(gRadioInitialized
        ? "[LoRa] TX suspenso: AUX nao ficou ocioso"
        : "[LoRa] TX suspenso: inicializacao/diagnostico AT nao confirmado; isso nao prova falha RF");
  } else if (encodeData(h, &rec, 1, frame, sizeof(frame), size) != Result::kOk) {
    Serial.println("[lora] falha ao codificar o quadro");
  } else {
    uint8_t attempts = 0;
    uint32_t rtt_us = 0;
    if (sendWithAck(frame, size, gFrameSeq, seq, attempts, rtt_us)) {
      lora_ok = true;
      ++gAckedCycles;
      Serial.printf("[LoRa] OK: gateway recebeu registro %lu e respondeu ACK | tentativa=%u rtt=%lu us\n",
                    static_cast<unsigned long>(seq), attempts,
                    static_cast<unsigned long>(rtt_us));
    } else {
      Serial.printf("[LoRa] NAO CONFIRMADO: sem ACK apos %u tentativas; verificar gateway e configuracao dos radios\n", attempts);
    }
  }
  ++gFrameSeq;

  Serial.printf("[RESUMO] RTC=%s | SHT interno=%s | SHT externo=%s | ADXL=%s | SD=%s | LoRa=%s\n",
                rtc_ok ? "CONTANDO/HORA NAO VALIDADA" : "FALHA", sht.in_ok ? "OK" : "FALHA",
                sht.out_ok ? "OK" : "FALHA", adxl_ok ? "OK" : "FALHA/ATENCAO",
                sd_ok ? "OK" : "FALHA", lora_ok ? "ACK OK" : "NAO CONFIRMADO");
  Serial.printf("[LoRa] ciclos confirmados=%lu/%lu\n",
                static_cast<unsigned long>(gAckedCycles), static_cast<unsigned long>(gSeq));

  Serial.printf("[cycle] ativo por %lu ms\n",
                static_cast<unsigned long>(millis() - cycle_start_ms));
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.printf("BeeCooler: diagnostico completo S3 | ciclo=%lu ms | ACK timeout=%lu ms\n",
                static_cast<unsigned long>(kCyclePeriodMs),
                static_cast<unsigned long>(kTestAckTimeoutMs));
  Serial.println("Gateway correspondente: comunicacao_wroom_test. Sem sleep ou cloud.");
  Serial.printf("[PINOS] SHT int SDA/SCL=%d/%d ext=%d/%d | SPI SCK/MISO/MOSI=%d/%d/%d CS ADXL/SD=%d/%d\n",
                kPinShtIntSda, kPinShtIntScl, kPinShtExtSda, kPinShtExtScl,
                kPinSpiSck, kPinSpiMiso, kPinSpiMosi, kPinAdxlCs, kPinSdCs);
  Serial.printf("[PINOS] RTC CE/IO/CLK=%d/%d/%d | VBAT=%d | LoRa RX/TX/M0/M1/AUX=%d/%d/%d/%d/%d\n",
                kPinRtcCe, kPinRtcIo, kPinRtcSclk, kPinVbat,
                kPinLoraRx, kPinLoraTx, kPinLoraM0, kPinLoraM1, kPinLoraAux);

  BeeCoolerLogic::parseBuildEpoch(__DATE__, __TIME__, gBuildEpoch);
  gBootId = static_cast<uint8_t>(esp_random() & 0xFF);
  Serial.printf("node=%u boot_id=%u\n", kNodeId, gBootId);

  // DS1302: sem hora valida, acerta com a da compilacao (a central de teste
  // nao tem NTP, entao o ACK nunca traz a hora).
  gClock.begin(kPinRtcCe, kPinRtcIo, kPinRtcSclk);
  uint32_t now = 0;
  if (!readClock(now)) {
    if (gBuildEpoch != 0) {
      gClock.writeEpoch(gBuildEpoch);
      uint32_t readback = 0;
      const bool set_ok = readClock(readback) && readback >= gBuildEpoch &&
                          readback - gBuildEpoch <= 2U;
      Serial.printf("[RTC] ajuste com hora da compilacao: %s (hora aproximada, sem NTP)\n",
                    set_ok ? "leitura de retorno OK" : "FALHA na leitura de retorno");
      if (!set_ok) printClockRegisters();
    }
  }

  BatteryMonitor::begin(kPinVbat);
  if (kPinCharging >= 0) pinMode(kPinCharging, INPUT_PULLUP);

  // Deselect both devices before any SPI transactions.
  pinMode(kPinAdxlCs, OUTPUT);
  digitalWrite(kPinAdxlCs, HIGH);
  pinMode(kPinSdCs, OUTPUT);
  digitalWrite(kPinSdCs, HIGH);
  sharedSpiBegin();
  gAdxl.begin(kPinAdxlCs, kPinAdxlInt1, kPinAdxlInt2);

  const size_t bytes = kAdxlWindowSamples * 3U * sizeof(int16_t);
  gWindow = static_cast<int16_t*>(
      heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (gWindow == nullptr) {
    gWindow = static_cast<int16_t*>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM));
  }
  if (gWindow == nullptr) Serial.println("[adxl] sem memoria para a janela");

  const BeeCoolerLora::Pins pins = {kPinLoraRx, kPinLoraTx, kPinLoraM0,
                                    kPinLoraM1, kPinLoraAux};
  Serial.printf("[LoRa] UART ESP<->modulo=%lu baud | monitor USB=115200 baud\n",
                static_cast<unsigned long>(BeeCoolerBench::kLoraBaud));
  Serial.println("[LoRa] SWITCH=0 confirmado na bancada: M0/M1 como entradas no ESP; controle por AT");
  gRadioInitialized = gLora.begin(Serial1, pins, BeeCoolerBench::kLoraBaud, false);
  if (!gRadioInitialized || !gLora.setMode(BeeCoolerLora::Mode::kNormal)) {
    Serial.println("[lora] AUX nao ficou ocioso ao entrar em modo normal");
  }
  if (gRadioInitialized) {
    gRadioInitialized = BeeCoolerBench::probeRadio(Serial1);
    gLora.flushInput();
  }
  Serial.println("[LoRa] inicializacao local nao prova enlace RF; somente ACK confirma comunicacao.");

  gNextCycleMs = millis();
}

void loop() {
  runCycle();
  gNextCycleMs += kCyclePeriodMs;
  const int32_t wait = static_cast<int32_t>(gNextCycleMs - millis());
  if (wait > 0) {
    delay(static_cast<uint32_t>(wait));
  } else {
    gNextCycleMs = millis();  // ciclo estourou 60 s: reancora
  }
}
