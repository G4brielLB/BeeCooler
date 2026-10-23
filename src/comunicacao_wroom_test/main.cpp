// BeeCooler - teste de comunicacao, na central (ESP32-WROOM).
//
// Versao simplificada da central da Fase 1: sem Wi-Fi, sem AWS, sem LittleFS.
// Recebe os quadros LoRa (BeeCoolerLink), responde com ACK e escreve tudo no
// monitor serial. Cada registro novo sai em formato legivel e como linha
// `BEEREC {json}` (mesmo formato do tools/gateway_collector.py).

#include <Arduino.h>

#include <BeeCoolerLink.h>
#include <BeeCoolerBenchConfig.h>
#include <BeeCoolerBenchRadio.h>
#include <BeeCoolerLogic.h>
#include <BeeCoolerLora.h>
#include <BeeCoolerRecord.h>

using namespace BeeCoolerLink;

namespace {

// Pinagem da central: docs/fase1-implementacao-e-testes.md, secao 4.2.
constexpr int kPinLoraRx = 16;   // ESP RX2 <- TXD do modulo
constexpr int kPinLoraTx = 17;   // ESP TX2 -> RXD do modulo
constexpr int kPinLoraM0 = 19;
constexpr int kPinLoraM1 = 21;
constexpr int kPinLoraAux = 18;
constexpr uint32_t kStatsPeriodMs = 15000U;
constexpr uint8_t kMaxNodes = 8;

struct NodeSeen {
  bool used;
  uint8_t node_id;
  uint8_t boot_id;
  uint32_t acked_up_to;
};

BeeCoolerLora gLora;
StreamParser gParser;
NodeSeen gNodes[kMaxNodes] = {};
uint32_t gFramesOk = 0;
uint32_t gAcksSent = 0;
uint32_t gAcksFailed = 0;
uint32_t gRecordsReceived = 0;
uint32_t gDuplicates = 0;
uint32_t gUartBytes = 0;
bool gRadioReady = false;
uint32_t gLastStatsMs = 0;

NodeSeen* nodeFor(uint8_t node_id, uint8_t boot_id) {
  NodeSeen* slot = nullptr;
  for (NodeSeen& n : gNodes) {
    if (n.used && n.node_id == node_id) {
      slot = &n;
      break;
    }
    if (!n.used && slot == nullptr) slot = &n;
  }
  if (slot == nullptr) return nullptr;
  if (!slot->used || slot->boot_id != boot_id) {  // no novo ou reiniciado
    slot->used = true;
    slot->node_id = node_id;
    slot->boot_id = boot_id;
    slot->acked_up_to = 0;
  }
  return slot;
}

void sendAck(const Frame& f, uint32_t acked_up_to) {
  if (!gRadioReady) {
    ++gAcksFailed;
    Serial.println("[ACK] FALHA: diagnostico UART/saida do modo AT nao confirmado");
    return;
  }
  Ack ack = {};
  ack.node_id = f.header.node_id;
  ack.frame_seq = f.header.frame_seq;
  ack.acked_up_to = acked_up_to;
  ack.epoch = 0;  // sem NTP: o no ignora
  uint8_t buffer[kAckSize];
  size_t size = 0;
  if (encodeAck(ack, buffer, sizeof(buffer), size) != Result::kOk) {
    ++gAcksFailed;
    Serial.println("[ACK] FALHA: codificacao");
    return;
  }
  uint32_t tx_end_us = 0;
  if (gLora.send(buffer, size, tx_end_us)) {
    ++gAcksSent;
    Serial.printf("[ACK] TX local OK: no=%u quadro=%u registro=%lu; conferir recepcao no S3\n",
                  ack.node_id, ack.frame_seq, static_cast<unsigned long>(acked_up_to));
  } else {
    ++gAcksFailed;
    Serial.println("[ACK] FALHA TX: verificar UART, AUX, modulo e alimentacao");
  }
}

void printSht(const char* label, int16_t temperature, uint16_t humidity, bool failed) {
  if (failed || temperature == BeeCoolerRecord::kInvalidI16 ||
      humidity == BeeCoolerRecord::kInvalidU16) {
    Serial.printf("     %s: FALHA informada pelo S3\n", label);
  } else {
    Serial.printf("     %s: %.2f C / %.2f %%RH\n", label,
                  temperature / 100.0f, humidity / 100.0f);
  }
}

void printRecord(uint8_t node, uint32_t seq, uint8_t attempt,
                 const BeeCoolerRecord::RecordV1& r) {
  Serial.printf("[rx] no=%u seq=%lu tentativa=%u ts=%lu\n", node,
                static_cast<unsigned long>(seq), attempt,
                static_cast<unsigned long>(r.ts));
  if (r.flags & BeeCoolerRecord::kRtcInvalid) {
    Serial.println("     RTC: FALHA; ts representa segundos desde o boot");
  } else {
    int y, mo, d, h, mi, s;
    BeeCoolerLogic::epochToCivil(r.ts, y, mo, d, h, mi, s);
    Serial.printf("     RTC do S3: %04d-%02d-%02d %02d:%02d:%02d (sem sincronizacao externa)\n",
                  y, mo, d, h, mi, s);
  }
  printSht("SHT30 interno", r.t_in, r.rh_in, r.flags & BeeCoolerRecord::kShtInFail);
  printSht("SHT30 externo", r.t_out, r.rh_out, r.flags & BeeCoolerRecord::kShtOutFail);
  Serial.printf("     bateria medida=%.3f V | SD do S3=%s | flags=0x%04X\n",
                r.vbat_mv / 1000.0f, (r.flags & BeeCoolerRecord::kSdFail) ? "FALHA" : "OK",
                r.flags);
  if ((r.flags & BeeCoolerRecord::kVibSkipped) ||
      r.vib_rms == BeeCoolerRecord::kInvalidU16 || r.vib_peak == BeeCoolerRecord::kInvalidU16) {
    Serial.println("     vibracao: FALHA/nao calculada");
  } else {
    Serial.printf("     vibracao: RMS=%.1f mg pico=%.1f mg | ADXL=%s FIFO=%s\n",
                  r.vib_rms / 10.0f, r.vib_peak / 10.0f,
                  (r.flags & BeeCoolerRecord::kAdxlFail) ? "FALHA" : "OK",
                  (r.flags & BeeCoolerRecord::kFifoOverrun) ? "PERDA DE AMOSTRAS" : "OK");
    Serial.printf("     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): %.2f %.2f %.2f %.2f\n",
                  BeeCoolerRecord::decodeBandDb(r.band_db[0]),
                  BeeCoolerRecord::decodeBandDb(r.band_db[1]),
                  BeeCoolerRecord::decodeBandDb(r.band_db[2]),
                  BeeCoolerRecord::decodeBandDb(r.band_db[3]));
  }
  Serial.printf("BEEREC {\"node\":%u,\"seq\":%lu,\"attempt\":%u,\"ts\":%lu,"
                "\"t_in\":%d,\"rh_in\":%u,\"t_out\":%d,\"rh_out\":%u,"
                "\"vbat_mv\":%u,\"vib_rms\":%u,\"vib_peak\":%u,"
                "\"bands\":[%u,%u,%u,%u],\"flags\":%u}\n",
                node, static_cast<unsigned long>(seq), attempt,
                static_cast<unsigned long>(r.ts), r.t_in, r.rh_in, r.t_out,
                r.rh_out, r.vbat_mv, r.vib_rms, r.vib_peak, r.band_db[0],
                r.band_db[1], r.band_db[2], r.band_db[3], r.flags);
}

void handleFrame(const Frame& f) {
  if (f.is_ack || f.header.node_id == 0 || f.header.node_id == 0xFF) return;
  NodeSeen* node = nodeFor(f.header.node_id, f.header.boot_id);
  if (node == nullptr) return;
  ++gFramesOk;
  Serial.printf("[LoRa RX] OK: quadro validado com CRC32 | no=%u boot=%u quadro=%u tentativa=%u\n",
                f.header.node_id, f.header.boot_id, f.header.frame_seq, f.header.attempt);

  switch (f.header.type) {
    case FrameType::kData: {
      for (uint8_t i = 0; i < f.header.n_rec; ++i) {
        const uint32_t seq = f.header.first_seq + i;
        if (seq <= node->acked_up_to) {
          ++gDuplicates;
          Serial.printf("[rx] no=%u seq=%lu (dup, tentativa=%u)\n",
                        f.header.node_id, static_cast<unsigned long>(seq),
                        f.header.attempt);
          continue;
        }
        BeeCoolerRecord::RecordV1 rec;
        memcpy(&rec, f.payload + i * BeeCoolerRecord::kRecordSize, sizeof(rec));
        printRecord(f.header.node_id, seq, f.header.attempt, rec);
        ++gRecordsReceived;
        node->acked_up_to = seq;
      }
      break;
    }
    case FrameType::kHello: {
      Hello h;
      if (decodeHello(f.payload, f.payload_length, h) == Result::kOk) {
        Serial.printf("[hello] no=%u fw=%04X reset=%u rtc_valid=%u last_seq=%lu "
                      "boots=%u\n",
                      f.header.node_id, h.fw_version, h.reset_reason,
                      h.rtc_valid, static_cast<unsigned long>(h.last_seq),
                      h.boot_count);
      }
      break;
    }
    case FrameType::kStatus: {
      Status s;
      if (decodeStatus(f.payload, f.payload_length, s) == Result::kOk) {
        Serial.printf("[status] no=%u backlog=%u vbat_tx=%u mV fw=%04X\n",
                      f.header.node_id, s.backlog, s.vbat_tx_mv, s.fw_version);
      }
      break;
    }
    default:
      return;  // EVENT: sem ACK
  }
  sendAck(f, node->acked_up_to);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("BeeCooler: teste de comunicacao WROOM (so serial, sem AWS)");

  const BeeCoolerLora::Pins pins = {kPinLoraRx, kPinLoraTx, kPinLoraM0,
                                    kPinLoraM1, kPinLoraAux};
  Serial.printf("[PINOS LoRa] RX=%d TX=%d M0=%d M1=%d AUX=%d | UART=%lu baud\n",
                kPinLoraRx, kPinLoraTx, kPinLoraM0, kPinLoraM1, kPinLoraAux,
                static_cast<unsigned long>(BeeCoolerBench::kLoraBaud));
  Serial.println("[LoRa] UART ESP<->modulo=9600 por padrao; monitor USB=115200 baud");
  Serial.println("[LoRa] SWITCH=0 confirmado na bancada: M0/M1 como entradas no ESP; controle por AT");
  const bool initialized = gLora.begin(Serial2, pins, BeeCoolerBench::kLoraBaud, false);
  if (!initialized || !gLora.setMode(BeeCoolerLora::Mode::kNormal)) {
    Serial.println("[lora] AUX nao ficou ocioso ao entrar em modo normal");
  }
  Serial.printf("[LoRa GPIO] M0=%d M1=%d AUX=%d RX=%d | M0/M1 lidos, nao comandados; AUX=0 quando ocioso\n",
                digitalRead(kPinLoraM0), digitalRead(kPinLoraM1),
                digitalRead(kPinLoraAux), digitalRead(kPinLoraRx));
  gRadioReady = initialized && BeeCoolerBench::probeRadio(Serial2);
  if (!gRadioReady) {
    Serial.println("[LoRa] diagnostico AT inconclusivo; ACK suspenso porque o modo do modulo nao foi confirmado");
  }
  gLora.flushInput();
  Serial.println("[LoRa] Enlace ainda NAO CONFIRMADO; aguarde RX com CRC32 e ACK recebido no S3.");
  gLastStatsMs = millis();
  Serial.println("aguardando quadros LoRa...");
}

void loop() {
  bool idle = true;
  while (gLora.available() > 0) {
    idle = false;
    const int value = gLora.read();
    if (value < 0) break;
    ++gUartBytes;
    bool got = gParser.feed(static_cast<uint8_t>(value));
    while (got) {
      Frame f;
      if (decodeFrame(gParser.frame(), gParser.frameSize(), f) == Result::kOk) {
        handleFrame(f);
      }
      got = gParser.poll();
    }
  }

  if (millis() - gLastStatsMs >= kStatsPeriodMs) {
    gLastStatsMs += kStatsPeriodMs;
    Serial.printf("[stats] quadros_ok=%lu crc_falhos=%lu bytes_descartados=%lu "
                  "acks_tx=%lu acks_falhos=%lu registros=%lu duplicados=%lu uart_bytes=%lu\n",
                  static_cast<unsigned long>(gFramesOk),
                  static_cast<unsigned long>(gParser.crcFailures()),
                  static_cast<unsigned long>(gParser.discardedBytes()),
                  static_cast<unsigned long>(gAcksSent),
                  static_cast<unsigned long>(gAcksFailed),
                  static_cast<unsigned long>(gRecordsReceived),
                  static_cast<unsigned long>(gDuplicates),
                  static_cast<unsigned long>(gUartBytes));
    if (gFramesOk == 0) {
      Serial.println("[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.");
    }
  }
  if (idle) delay(2);
}
