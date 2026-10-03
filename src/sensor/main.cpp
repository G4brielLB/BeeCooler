// BeeCooler - firmware do no sensor (ESP32-S3), Fase 1.
//
// Segue a Arquitetura de Software v1.2. Cada wake executa UM ciclo e volta ao
// deep sleep (setup() faz tudo; loop() nunca roda):
//
//   boot -> DS1302 + vbat -> politica de energia -> SHT30 -> janela ADXL345 ->
//   features -> microSD -> ring na flash + RAM do DS1302 -> LoRa (se for o
//   slot) -> deep sleep ancorado no proximo slot do DS1302.
//
// O mesmo binario serve as Fases 1 e 2: o microSD e detectado em tempo de
// execucao e a falta dele nunca derruba a coleta (D50, D52).

#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <esp_sleep.h>
#include <esp_system.h>

#include <BeeCoolerLink.h>
#include <BeeCoolerLogic.h>
#include <BeeCoolerLora.h>
#include <BeeCoolerRecord.h>
#include <BeeCoolerVib.h>

#include "Adxl345Fifo.h"
#include "BatteryMonitor.h"
#include "Console.h"
#include "Ds1302Clock.h"
#include "FlashRing.h"
#include "LoraSession.h"
#include "NodeState.h"
#include "RawSink.h"
#include "SharedSpi.h"
#include "Sht30Pair.h"
#include "config.h"
#include "pins.h"

using namespace BeeCoolerRecord;
using BeeCoolerLogic::LevelParams;

namespace {

constexpr uint8_t kNodeId = BEE_NODE_ID;

Ds1302Clock gClock;
FlashRing gRing;
BeeCoolerLora gLora;
Sht30Pair gSht;
Adxl345Fifo gAdxl;

uint32_t buildEpoch() {
  static uint32_t cached = 0;
  if (cached == 0) BeeCoolerLogic::parseBuildEpoch(__DATE__, __TIME__, cached);
  return cached;
}

// ---- Relogio ---------------------------------------------------------------

// Hora do DS1302 apenas se plausivel (nao anterior a compilacao do firmware).
bool readClock(uint32_t& epoch) {
  return gClock.readEpoch(epoch) && epoch >= buildEpoch();
}

// ---- Estado duravel na RAM do DS1302 --------------------------------------

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
  gState.power.level = ds_valid ? (ds.power_level & 0x3) : 0;
  gState.last_sync_epoch = ds_valid ? ds.last_sync_epoch : 0;
  setPendingFlags(pendingFlags() | kColdBoot);
  saveDsState();
}

// ---- Vibracao ----------------------------------------------------------------

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

// ---- Console serial de logs --------------------------------------------------

void logLine(const char* text) { Serial.println(text); }

// ---- Politica de energia ------------------------------------------------------

uint16_t restingVbat() {
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

uint16_t filteredVbat() {
  const auto& h = gState.vbat_history;
  if (gState.vbat_history_count < 3) return h[gState.vbat_history_count - 1];
  return BeeCoolerLogic::median3(h[0], h[1], h[2]);
}

// ---- Espera ate o slot LoRa do no -----------------------------------------------

void lightSleepSeconds(uint32_t seconds, uint32_t extra_ms) {
  const uint64_t us = static_cast<uint64_t>(seconds) * 1000000ULL +
                      static_cast<uint64_t>(extra_ms) * 1000ULL;
  if (us == 0) return;
  Serial.flush();
  esp_sleep_enable_timer_wakeup(us);
  esp_light_sleep_start();
}

// ---- Deep sleep ---------------------------------------------------------------

[[noreturn]] void sleepUntilNextSlot(const LevelParams& params, bool rtc_valid,
                                     uint32_t cycle_start_ms) {
  uint32_t seconds;
  uint32_t now = 0;
  if (rtc_valid && readClock(now)) {
    seconds = BeeCoolerLogic::sleepSeconds(now, params.cycle_period_s,
                                           kBootMarginS);
  } else {
    // Sem hora valida o calendario nao pode ser seguido: periodo fixo.
    const uint32_t elapsed = (millis() - cycle_start_ms) / 1000U;
    seconds = params.cycle_period_s > elapsed + 1 ? params.cycle_period_s - elapsed : 1;
  }

  // O modulo LoRa fica em sleep (M0/M1 altos) durante o deep sleep. Apenas
  // GPIO RTC (0-21) seguram nivel; por isso M0/M1 estao em 1 e 2.
  gpio_set_level(static_cast<gpio_num_t>(kPinLoraM0), 1);
  gpio_set_level(static_cast<gpio_num_t>(kPinLoraM1), 1);
  gpio_hold_en(static_cast<gpio_num_t>(kPinLoraM0));
  gpio_hold_en(static_cast<gpio_num_t>(kPinLoraM1));
  gpio_deep_sleep_hold_en();

  Serial.printf("[sleep] %lu s\n", static_cast<unsigned long>(seconds));
  Serial.flush();
  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(seconds) * 1000000ULL);
  esp_deep_sleep_start();
  for (;;) {}  // nunca alcancado
}

// ---- Um ciclo -----------------------------------------------------------------

void runCycle() {
  const uint32_t cycle_start_ms = millis();
  Serial.begin(115200);
  // Sem PC na USB-CDC, uma escrita com o buffer cheio bloquearia por ~100 ms e
  // gastaria energia (RNF-07): descarta em vez de esperar.
  Serial.setTxTimeoutMs(0);

  const bool warm = nodeStateInit();  // false: cold boot (reset ou energia)
  const uint8_t reset_reason = static_cast<uint8_t>(esp_reset_reason());

  // Solta o hold que manteve o LoRa em sleep durante o deep sleep.
  gpio_hold_dis(static_cast<gpio_num_t>(kPinLoraM0));
  gpio_hold_dis(static_cast<gpio_num_t>(kPinLoraM1));
  gpio_deep_sleep_hold_dis();

  gClock.begin(kPinRtcCe, kPinRtcIo, kPinRtcSclk);
  const bool ring_ok = gRing.begin(kRingPartitionLabel, kRingPartitionSubtype);
  BatteryMonitor::begin(kPinVbat);
  if (kPinCharging >= 0) pinMode(kPinCharging, INPUT_PULLUP);

  BeeCoolerLora::Pins lora_pins = {kPinLoraRx, kPinLoraTx, kPinLoraM0,
                                   kPinLoraM1, kPinLoraAux};
  gLora.begin(Serial1, lora_pins, kLoraBaud);
  LoraSession session(gLora, gRing, gClock, kNodeId);

  if (!warm) {
    recoverAfterColdBoot(ring_ok);
    Serial.printf("[boot] cold boot #%u reset=%u last_seq=%lu ring=%s fw=%04X\n",
                  gState.boot_count, reset_reason,
                  static_cast<unsigned long>(gState.last_seq),
                  ring_ok ? "ok" : "FALHA", kFirmwareVersion);
    Console::maybeRun(kMaintenanceWindowMs, gClock, gRing);
  }

  uint32_t now = 0;
  bool rtc_valid = readClock(now);

  // HELLO imediato no cold boot; o ACK corrige o relogio se ele estiver
  // invalido ou desviado (UC-06, UC-10).
  if (!warm) {
    bool adjusted = false;
    const bool acked = session.hello(reset_reason, rtc_valid, adjusted);
    Serial.printf("[hello] %s\n", acked ? "ACK" : "sem ACK");
    if (adjusted) setPendingFlags(pendingFlags() | kClockAdjusted);
    rtc_valid = readClock(now);
    saveDsState();
  }

  // ---- Tensao da bateria e nivel de energia (sempre antes de qualquer TX)
  const uint16_t vbat_mv = restingVbat();
  const bool level_changed =
      BeeCoolerLogic::powerUpdate(gState.power, filteredVbat());
  const LevelParams params =
      BeeCoolerLogic::levelParams(gState.power.level, kTimeDivisor);
  if (level_changed) {
    Serial.printf("[power] nivel %u (vbat=%u mV)\n", gState.power.level, vbat_mv);
  }

  // ---- Slot do ciclo
  const uint32_t slot =
      rtc_valid ? BeeCoolerLogic::nearestSlot(now, params.cycle_period_s) : 0;

  RecordV1 record;
  initInvalid(record);
  uint16_t flags = pendingFlags();
  setPendingFlags(0);
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
      for (uint8_t n = 0; n < 3; ++n) {
        ext.sht_raw[s * 6 + n * 2] = sht.raw[s][n][0];
        ext.sht_raw[s * 6 + n * 2 + 1] = sht.raw[s][n][1];
      }
    }
    BeeCoolerLogic::stuckPush(gState.stuck, sht.t_in, sht.rh_in, sht.t_out);
    if (BeeCoolerLogic::stuckDetected(gState.stuck)) flags |= kShtInStuck;
  }

  // ---- Janela de vibracao (nivel 0 apenas)
  VibCapture vib;
  sharedSpiBegin();
  gAdxl.begin(kPinAdxlCs, kPinAdxlInt1, kPinAdxlInt2);
  if (params.measure_vibration) {
    vib.buffer = allocateWindowBuffer(kAdxlWindowSamples);
    if (vib.buffer == nullptr) {
      flags |= kVibSkipped;
    } else {
      vib.window = gAdxl.capture(vib.buffer, kAdxlWindowSamples,
                                 kAdxlWindowTimeoutMs, kMechEventEnabled);
      if (!vib.window.device_ok) {
        flags |= kAdxlFail | kVibSkipped;
      } else {
        if (vib.window.overruns > 0) flags |= kFifoOverrun;
        if (vib.window.activity) flags |= kMechEvent;
        if (vib.window.samples >= BeeCoolerVib::kSegment) {
          const uint32_t t0 = millis();
          vib.features = BeeCoolerVib::compute(
              vib.buffer, vib.window.samples, static_cast<float>(kAdxlOdrHz));
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

  flags = setPowerLevel(flags, gState.power.level);
  const uint32_t seq = gState.last_seq + 1;

  // ---- microSD: RAW + registro estendido (so Fase 1; falha nao derruba nada)
  SdRawSink sd;
  NullRawSink null_sink;
  RawSink* sink = &null_sink;
  uint32_t sd_write_ms = 0;
  if (kSdEnabled) {
    if (sd.begin(kPinSdCs, kNodeId, slot)) {
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
      header.node_id = kNodeId;
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
    ext.vbat_tx_mv = gState.vbat_tx_mv;
    sd_write_ms = millis() - t0;
    ext.sd_write_ms = static_cast<uint16_t>(min<uint32_t>(0xFFFF, sd_write_ms));
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

  // ---- Persistir antes de qualquer transmissao (RF-04)
  record.flags = flags;
  bool stored = false;
  if (ring_ok && gRing.append(seq, record)) {
    gState.last_seq = seq;
    stored = true;
    saveDsState();
  } else {
    Serial.println("[ring] FALHA ao gravar o registro");
    if (kSdEnabled && sink->available()) sink->logEvent("ring append failed");
  }
  Serial.printf("[rec] seq=%lu slot=%lu t_in=%d rh_in=%u t_out=%d rh_out=%u "
                "vbat=%u rms=%u peak=%u flags=0x%04X %s\n",
                static_cast<unsigned long>(seq), static_cast<unsigned long>(slot),
                record.t_in, record.rh_in, record.t_out, record.rh_out,
                record.vbat_mv, record.vib_rms, record.vib_peak, record.flags,
                stored ? "" : "(NAO GRAVADO)");

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
    if (rtc_valid) {
      // t_tx = slot + 20 s + (node_id - 1) * 30 s + jitter(0-2 s)
      uint32_t t = 0;
      const uint32_t target = slot + BeeCoolerLogic::loraOffsetSeconds(kNodeId);
      if (readClock(t) && target > t) {
        lightSleepSeconds(target - t, random(0, kSlotJitterMaxMs + 1));
      }
    }
    SessionInfo info = {gState.sd_free_mb, gState.sd_errors, reset_reason};
    const SessionResult r = session.run(record, params.status_only, info);
    gState.last_lora_slot = slot;
    gState.cycles_since_lora = 0;
    if (r.clock_adjusted) setPendingFlags(pendingFlags() | kClockAdjusted);
    saveDsState();

    char line[128];
    snprintf(line, sizeof(line),
             "lora status=%d done=%d frames=%u acked=%lu retx=%lu rtt_us=%lu..%lu",
             r.status_acked, r.completed, r.frames_sent,
             static_cast<unsigned long>(r.acked_up_to),
             static_cast<unsigned long>(r.retransmissions),
             static_cast<unsigned long>(r.min_rtt_us),
             static_cast<unsigned long>(r.max_rtt_us));
    Serial.printf("[%s]\n", line);
    if (sink->available()) sink->logEvent(line);
    if (r.clock_adjusted) rtc_valid = readClock(now) || rtc_valid;
  }

#ifdef BEE_TEST_FAST
  if (sink->available()) sink->logEvent("TEST_FAST: registros fora do dataset");
#endif
  sink->end();
  gSht.end();

  Serial.printf("[cycle] acordado por %lu ms\n",
                static_cast<unsigned long>(millis() - cycle_start_ms));
  sleepUntilNextSlot(params, rtc_valid, cycle_start_ms);
}

}  // namespace

void setup() { runCycle(); }

void loop() {}
