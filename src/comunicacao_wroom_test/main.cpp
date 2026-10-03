// BeeCooler - teste de comunicacao, na central (ESP32-WROOM).
//
// Versao simplificada da central da Fase 1: sem Wi-Fi, sem AWS, sem LittleFS.
// Recebe os quadros LoRa (BeeCoolerLink), responde com ACK e escreve tudo no
// monitor serial. Cada registro novo sai em formato legivel e como linha
// `BEEREC {json}` (mesmo formato do tools/gateway_collector.py).

#include <Arduino.h>

#include <BeeCoolerLink.h>
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
constexpr uint32_t kLoraBaud = 115200U;  // igual ao no
constexpr uint32_t kStatsPeriodMs = 60000U;
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
  Ack ack = {};
  ack.node_id = f.header.node_id;
  ack.frame_seq = f.header.frame_seq;
  ack.acked_up_to = acked_up_to;
  ack.epoch = 0;  // sem NTP: o no ignora
  uint8_t buffer[kAckSize];
  size_t size = 0;
  if (encodeAck(ack, buffer, sizeof(buffer), size) != Result::kOk) return;
  uint32_t tx_end_us = 0;
  if (gLora.send(buffer, size, tx_end_us)) ++gAcksSent;
}

void printRecord(uint8_t node, uint32_t seq, uint8_t attempt,
                 const BeeCoolerRecord::RecordV1& r) {
  Serial.printf("[rx] no=%u seq=%lu tentativa=%u ts=%lu\n", node,
                static_cast<unsigned long>(seq), attempt,
                static_cast<unsigned long>(r.ts));
  Serial.printf("     interno: %.2f C / %.2f %%   externo: %.2f C / %.2f %%\n",
                r.t_in / 100.0f, r.rh_in / 100.0f, r.t_out / 100.0f,
                r.rh_out / 100.0f);
  Serial.printf("     vbat=%.3f V  vib rms=%.1f mg peak=%.1f mg  "
                "bandas=%u,%u,%u,%u  flags=0x%04X\n",
                r.vbat_mv / 1000.0f, r.vib_rms / 10.0f, r.vib_peak / 10.0f,
                r.band_db[0], r.band_db[1], r.band_db[2], r.band_db[3],
                r.flags);
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

  switch (f.header.type) {
    case FrameType::kData: {
      for (uint8_t i = 0; i < f.header.n_rec; ++i) {
        const uint32_t seq = f.header.first_seq + i;
        if (seq <= node->acked_up_to) {
          Serial.printf("[rx] no=%u seq=%lu (dup, tentativa=%u)\n",
                        f.header.node_id, static_cast<unsigned long>(seq),
                        f.header.attempt);
          continue;
        }
        BeeCoolerRecord::RecordV1 rec;
        memcpy(&rec, f.payload + i * BeeCoolerRecord::kRecordSize, sizeof(rec));
        printRecord(f.header.node_id, seq, f.header.attempt, rec);
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
  gLora.begin(Serial2, pins, kLoraBaud);
  if (!gLora.setMode(BeeCoolerLora::Mode::kNormal)) {
    Serial.println("[lora] AUX nao ficou ocioso ao entrar em modo normal");
  }
  gLastStatsMs = millis();
  Serial.println("aguardando quadros LoRa...");
}

void loop() {
  bool idle = true;
  while (gLora.available() > 0) {
    idle = false;
    bool got = gParser.feed(static_cast<uint8_t>(gLora.read()));
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
                  "acks=%lu\n",
                  static_cast<unsigned long>(gFramesOk),
                  static_cast<unsigned long>(gParser.crcFailures()),
                  static_cast<unsigned long>(gParser.discardedBytes()),
                  static_cast<unsigned long>(gAcksSent));
  }
  if (idle) delay(2);
}
