#include "RadioTask.h"

#include <BeeCoolerLink.h>
#include <BeeCoolerLora.h>
#include <BeeCoolerRecord.h>

#include "GatewayStore.h"
#include "UplinkTask.h"
#include "config.h"
#include "pins.h"

using namespace BeeCoolerLink;

namespace RadioTask {

namespace {

BeeCoolerLora gLora;
StreamParser gParser;
Stats gStats = {};
volatile uint32_t gLastActivityMs = 0;

#ifdef BEE_GATEWAY_FAULT_INJECTION
volatile uint32_t gCorruptEvery = 0;
volatile uint32_t gDropAcks = 0;
volatile bool gRebootAfterReceive = false;
uint32_t gReceived = 0;
#endif

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
  ack.epoch = Uplink::epochOrZero();  // 0 while NTP is not valid: the node rejects it
  // TODO(P-HDR): the module does not report RSSI/SNR in transparent mode; they
  // need AT+DRSSI or an appended RSSI byte. 0 means "unknown".
  ack.rssi = 0;
  ack.snr = 0;
  ack.pending_command = 0;  // UC-15 is Phase 2

  uint8_t buffer[kAckSize];
  size_t size = 0;
  if (encodeAck(ack, buffer, sizeof(buffer), size) != Result::kOk) return;
  uint32_t tx_end_us = 0;
  if (gLora.send(buffer, size, tx_end_us)) {
    ++gStats.acks_sent;
  }
  gLastActivityMs = millis();
}

// Machine-readable echo for the bench collector (D55).
void echoRecord(uint8_t node, uint32_t seq, uint8_t attempt,
                const BeeCoolerRecord::RecordV1& r) {
  Serial.printf("BEEREC {\"node\":%u,\"seq\":%lu,\"attempt\":%u,\"ts\":%lu,"
                "\"t_in\":%d,\"rh_in\":%u,\"t_out\":%d,\"rh_out\":%u,"
                "\"vbat_mv\":%u,\"vib_rms\":%u,\"vib_peak\":%u,"
                "\"bands\":[%u,%u,%u,%u],\"flags\":%u}\n",
                node, static_cast<unsigned long>(seq), attempt,
                static_cast<unsigned long>(r.ts), r.t_in, r.rh_in, r.t_out,
                r.rh_out, r.vbat_mv, r.vib_rms, r.vib_peak, r.band_db[0],
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
  item.rx_time = Uplink::epochOrZero();
  return item;
}

void bumpHistogram(NodeRecordState& n, uint8_t attempt) {
  if (attempt >= 1 && attempt <= 4) ++n.attempt_histogram[attempt];
}

// 7.1: validate -> idempotent insert -> persist -> update acked_up_to -> ACK.
void handleFrame(const Frame& f) {
  if (f.is_ack || f.header.node_id == 0 || f.header.node_id == 0xFF) return;
  NodeRecordState* node = GatewayStore::node(f.header.node_id);
  if (node == nullptr) return;  // more nodes than kMaxNodes
  ++gStats.frames_ok;

#ifdef BEE_GATEWAY_FAULT_INJECTION
  if (gRebootAfterReceive) {
    Serial.println("[fault] reiniciando entre recepcao e persistencia");
    Serial.flush();
    ESP.restart();
  }
#endif

  bool persisted = true;
  const uint32_t before = node->acked_up_to;

  switch (f.header.type) {
    case FrameType::kData: {
      const uint32_t first = f.header.first_seq;
      const uint32_t last = first + f.header.n_rec - 1U;
      if (last <= node->acked_up_to) {
        gStats.duplicate_records += f.header.n_rec;  // already persisted: re-ACK only
      } else if (first > node->acked_up_to + 1U) {
        ++gStats.gap_frames;  // the node resends from acked_up_to + 1
      } else {
        for (uint8_t i = 0; i < f.header.n_rec && persisted; ++i) {
          const uint32_t seq = first + i;
          if (seq <= node->acked_up_to) continue;
          QueueItem item = makeItem(QueueKind::kRecord, f, seq);
          memcpy(item.data, f.payload + i * BeeCoolerRecord::kRecordSize,
                 BeeCoolerRecord::kRecordSize);
          persisted = GatewayStore::appendItem(item);
          if (persisted) {
            node->acked_up_to = seq;
            ++gStats.records_stored;
            BeeCoolerRecord::RecordV1 rec;
            memcpy(&rec, item.data, sizeof(rec));
            echoRecord(f.header.node_id, seq, f.header.attempt, rec);
          }
        }
      }
      bumpHistogram(*node, f.header.attempt);
      break;
    }
    case FrameType::kStatus: {
      const uint32_t key = (static_cast<uint32_t>(f.header.boot_id) << 16) |
                           f.header.frame_seq;
      if (key != node->last_status_boot_key) {  // retransmissions reuse frame_seq
        QueueItem item = makeItem(QueueKind::kStatus, f, f.header.first_seq);
        memcpy(item.data, f.payload, kStatusPayloadSize);
        persisted = GatewayStore::appendItem(item);
        if (persisted) {
          node->last_status_boot_key = key;
          ++gStats.status_stored;
        }
      }
      bumpHistogram(*node, f.header.attempt);
      break;
    }
    case FrameType::kHello: {
      Hello h;
      if (decodeHello(f.payload, f.payload_length, h) == Result::kOk) {
        ++gStats.hello_seen;
        Serial.printf("[hello] node=%u fw=%04X reset=%u rtc_valid=%u last_seq=%lu "
                      "boots=%u (acked_up_to=%lu)\n",
                      f.header.node_id, h.fw_version, h.reset_reason, h.rtc_valid,
                      static_cast<unsigned long>(h.last_seq), h.boot_count,
                      static_cast<unsigned long>(node->acked_up_to));
      }
      break;
    }
    default:
      return;  // EVENT is Phase 2; no ACK
  }

  node->last_boot_id = f.header.boot_id;
  node->last_rx_epoch = Uplink::epochOrZero();
  // The state must be durable before the ACK: it holds acked_up_to.
  persisted = GatewayStore::saveNode(*node) && persisted;
  if (!persisted) {
    ++gStats.persist_failures;
    node->acked_up_to = before;  // never confirm what is not stored
    return;                      // no ACK: the node retransmits
  }
  sendAck(f, node->acked_up_to);
}

void radioLoop(void*) {
  for (;;) {
    bool idle = true;
    while (gLora.available() > 0) {
      idle = false;
      const uint8_t byte = static_cast<uint8_t>(gLora.read());
      bool got = gParser.feed(byte);
      while (got) {
        Frame f;
        if (decodeFrame(gParser.frame(), gParser.frameSize(), f) == Result::kOk) {
#ifdef BEE_GATEWAY_FAULT_INJECTION
          // Same observable effect as a corrupted CRC: no record, no ACK.
          if (gCorruptEvery > 0 && ++gReceived % gCorruptEvery == 0U) {
            Serial.println("[fault] frame tratado como corrompido");
            got = gParser.poll();
            continue;
          }
#endif
          gLastActivityMs = millis();
          handleFrame(f);
        }
        got = gParser.poll();
      }
    }
    gStats.crc_failures = gParser.crcFailures();
    gStats.discarded_bytes = gParser.discardedBytes();
    if (idle) vTaskDelay(pdMS_TO_TICKS(2));
  }
}

}  // namespace

void start() {
  BeeCoolerLora::Pins pins = {kPinLoraRx, kPinLoraTx, kPinLoraM0, kPinLoraM1,
                              kPinLoraAux};
  gLora.begin(Serial2, pins, kLoraBaud);
  gLora.setMode(BeeCoolerLora::Mode::kNormal);  // always listening
  xTaskCreatePinnedToCore(radioLoop, "task_radio", kRadioTaskStack, nullptr,
                          kRadioTaskPriority, nullptr, kRadioTaskCore);
}

Stats stats() { return gStats; }
uint32_t lastActivityMs() { return gLastActivityMs; }

#ifdef BEE_GATEWAY_FAULT_INJECTION
void faultCorruptEvery(uint32_t n) { gCorruptEvery = n; }
void faultDropAcks(uint32_t count) { gDropAcks = count; }
void faultRebootAfterReceive(bool on) { gRebootAfterReceive = on; }
#endif

}  // namespace RadioTask
