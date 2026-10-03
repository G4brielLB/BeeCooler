// BeeCooler - teste de comunicacao, no sensor (ESP32-S3).
//
// Versao simplificada do firmware da Fase 1: sem deep sleep, sem ring, sem
// microSD. A cada 60 s le os sensores (SHT30, ADXL345, bateria, DS1302) e
// envia um quadro DATA por LoRa (protocolo real BeeCoolerLink, com ACK e ate
// 4 tentativas). Tudo que acontece sai no monitor serial.

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_random.h>

#include <BeeCoolerLink.h>
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

constexpr uint32_t kCyclePeriodMs = 60000U;
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

bool readClock(uint32_t& epoch) {
  return gClock.readEpoch(epoch) && epoch >= gBuildEpoch;
}

// Uma tentativa por vez, ate kMaxAttempts. O cronometro do ACK comeca quando
// o AUX cai (fim do TX), como em LoraSession::exchange.
bool sendWithAck(uint8_t* frame, size_t size, uint16_t frame_seq,
                 uint8_t& attempts, uint32_t& rtt_us) {
  for (uint8_t attempt = 1; attempt <= kMaxAttempts; ++attempt) {
    attempts = attempt;
    if (setAttempt(frame, size, attempt) != Result::kOk) return false;

    gLora.flushInput();
    gParser.reset();
    uint32_t tx_end_us = 0;
    if (gLora.send(frame, size, tx_end_us)) {
      while ((micros() - tx_end_us) < kAckTimeoutMs * 1000UL) {
        while (gLora.available() > 0) {
          bool got = gParser.feed(static_cast<uint8_t>(gLora.read()));
          while (got) {
            Frame decoded;
            if (decodeFrame(gParser.frame(), gParser.frameSize(), decoded) ==
                    Result::kOk &&
                decoded.is_ack && decoded.ack.node_id == kNodeId &&
                decoded.ack.frame_seq == frame_seq) {
              rtt_us = micros() - tx_end_us;
              return true;
            }
            got = gParser.poll();
          }
        }
        delay(1);
      }
    }
    if (attempt < kMaxAttempts) {
      delay(random(kBackoffMinMs, kBackoffMaxMs + 1));
    }
  }
  return false;
}

void runCycle() {
  const uint32_t cycle_start_ms = millis();

  RecordV1 rec;
  initInvalid(rec);
  uint16_t flags = 0;

  // ---- DS1302
  uint32_t now = 0;
  if (readClock(now)) {
    rec.ts = now;
  } else {
    flags |= kRtcInvalid;
    rec.ts = cycle_start_ms / 1000U;  // segundos desde o boot
  }

  // ---- Bateria
  rec.vbat_mv = BatteryMonitor::readMv();
  if (kPinCharging >= 0 && digitalRead(kPinCharging) == LOW) flags |= kCharging;

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

  // ---- ADXL345 (janela de 10 s)
  if (gWindow == nullptr) {
    flags |= kVibSkipped;
  } else {
    Serial.println("[adxl] capturando janela de 10 s...");
    Serial.flush();
    const AdxlWindow w = gAdxl.capture(gWindow, kAdxlWindowSamples,
                                       kAdxlWindowTimeoutMs, false);
    if (!w.device_ok) {
      flags |= kAdxlFail | kVibSkipped;
    } else {
      if (w.overruns > 0) flags |= kFifoOverrun;
      BeeCoolerVib::Features f = {};
      if (w.samples >= BeeCoolerVib::kSegment) {
        f = BeeCoolerVib::compute(gWindow, w.samples,
                                  static_cast<float>(kAdxlOdrHz));
      }
      if (f.valid) {
        rec.vib_rms = BeeCoolerVib::toRecordMg10(f.rms_mg);
        rec.vib_peak = BeeCoolerVib::toRecordMg10(f.peak_mg);
        for (size_t b = 0; b < BeeCoolerVib::kBands; ++b) {
          rec.band_db[b] = encodeBandDb(f.band_db[b]);
        }
      } else {
        flags |= kVibSkipped;
      }
    }
  }
  rec.flags = flags;

  const uint32_t seq = ++gSeq;
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
  if (encodeData(h, &rec, 1, frame, sizeof(frame), size) != Result::kOk) {
    Serial.println("[lora] falha ao codificar o quadro");
  } else {
    uint8_t attempts = 0;
    uint32_t rtt_us = 0;
    if (sendWithAck(frame, size, gFrameSeq, attempts, rtt_us)) {
      Serial.printf("[lora] ACK tentativa=%u rtt=%lu us\n", attempts,
                    static_cast<unsigned long>(rtt_us));
    } else {
      Serial.printf("[lora] sem ACK apos %u tentativas\n", attempts);
    }
  }
  ++gFrameSeq;

  Serial.printf("[cycle] ativo por %lu ms\n",
                static_cast<unsigned long>(millis() - cycle_start_ms));
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("BeeCooler: teste de comunicacao S3 (ciclo de 60 s)");

  BeeCoolerLogic::parseBuildEpoch(__DATE__, __TIME__, gBuildEpoch);
  gBootId = static_cast<uint8_t>(esp_random() & 0xFF);
  Serial.printf("node=%u boot_id=%u\n", kNodeId, gBootId);

  // DS1302: sem hora valida, acerta com a da compilacao (a central de teste
  // nao tem NTP, entao o ACK nunca traz a hora).
  gClock.begin(kPinRtcCe, kPinRtcIo, kPinRtcSclk);
  uint32_t now = 0;
  if (!readClock(now)) {
    gClock.writeEpoch(gBuildEpoch);
    Serial.println("[rtc] relogio invalido: acertado com a hora da compilacao");
  }

  BatteryMonitor::begin(kPinVbat);
  if (kPinCharging >= 0) pinMode(kPinCharging, INPUT_PULLUP);

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
  gLora.begin(Serial1, pins, kLoraBaud);
  if (!gLora.setMode(BeeCoolerLora::Mode::kNormal)) {
    Serial.println("[lora] AUX nao ficou ocioso ao entrar em modo normal");
  }

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
