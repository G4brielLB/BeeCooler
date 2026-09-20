#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <BeeCoolerIntegrationTest.h>
#include <BeeCoolerProtocol.h>

#include <string.h>

namespace {

using BeeCoolerProtocol::Packet;
using BeeCoolerProtocol::Status;

constexpr uint8_t kSensorPeerMac[ESP_NOW_ETH_ALEN] = {
    SENSOR_MAC_B0, SENSOR_MAC_B1, SENSOR_MAC_B2,
    SENSOR_MAC_B3, SENSOR_MAC_B4, SENSOR_MAC_B5
};
constexpr uint8_t kTestWifiChannel = 6U;
constexpr size_t kMaximumBatchFragments = 8U;
constexpr size_t kMaximumBatchBytes =
    BeeCoolerIntegrationTest::kNormalBatchWireSize;
constexpr size_t kReceiveQueueDepth = 16U;

struct ReceivedFrame {
  uint8_t sender_mac[ESP_NOW_ETH_ALEN];
  uint16_t length;
  uint8_t bytes[BeeCoolerProtocol::kMaxSerializedPacketSize];
};

struct CommunicationStatistics {
  uint32_t valid_data_packets;
  uint32_t unique_fragments;
  uint32_t duplicate_fragments;
  uint32_t malformed_packets;
  uint32_t crc_failures;
  uint32_t acks_transmitted;
  uint32_t completed_batches;
  uint32_t incomplete_batches_replaced;
  uint32_t sequence_gaps;
  uint32_t receive_queue_drops;
};

struct BatchState {
  bool active;
  uint32_t node_id;
  uint32_t batch_id;
  uint16_t expected_fragments;
  uint16_t received_fragments;
  uint16_t duplicate_count;
  bool present[kMaximumBatchFragments];
  Packet fragments[kMaximumBatchFragments];
};

QueueHandle_t receiveQueue = nullptr;
volatile uint32_t callbackQueueDrops = 0U;
bool radioReady = false;
uint32_t nextAckSequenceId = 1U;
bool haveLastUniqueSequence = false;
uint32_t lastUniqueSequence = 0U;
CommunicationStatistics statistics = {};
BatchState batchState = {};
BatchState lastCompletedBatch = {};

bool isPeerMacConfigured(const uint8_t* mac) {
  bool all_zero = true;
  bool all_ff = true;
  for (size_t i = 0U; i < ESP_NOW_ETH_ALEN; ++i) {
    all_zero = all_zero && mac[i] == 0U;
    all_ff = all_ff && mac[i] == 0xFFU;
  }
  return !all_zero && !all_ff;
}

void printMac(const uint8_t* mac) {
  for (size_t i = 0U; i < ESP_NOW_ETH_ALEN; ++i) {
    if (i != 0U) Serial.print(':');
    if (mac[i] < 0x10U) Serial.print('0');
    Serial.print(mac[i], HEX);
  }
}

bool checkEsp(const char* operation, esp_err_t result) {
  Serial.printf("RADIO_CONFIG operation=%s result=%s code=%d\n", operation,
                esp_err_to_name(result), static_cast<int>(result));
  return result == ESP_OK;
}

void onDataReceived(const uint8_t* mac, const uint8_t* data, int length) {
  if (receiveQueue == nullptr || mac == nullptr || data == nullptr ||
      length <= 0 ||
      length > static_cast<int>(BeeCoolerProtocol::kMaxSerializedPacketSize)) {
    ++callbackQueueDrops;
    return;
  }
  ReceivedFrame frame = {};
  memcpy(frame.sender_mac, mac, ESP_NOW_ETH_ALEN);
  frame.length = static_cast<uint16_t>(length);
  memcpy(frame.bytes, data, static_cast<size_t>(length));
  if (xQueueSend(receiveQueue, &frame, 0U) != pdTRUE) {
    ++callbackQueueDrops;
  }
}

bool initializeRadio() {
  const bool station_mode_ok = WiFi.mode(WIFI_STA);
  Serial.printf("RADIO_CONFIG operation=wifi_station_mode result=%s\n",
                station_mode_ok ? "OK" : "FAILED");
  const bool disconnect_ok = WiFi.disconnect();
  Serial.printf("RADIO_CONFIG operation=wifi_disconnect result=%s\n",
                disconnect_ok ? "OK" : "FAILED");
  if (!station_mode_ok || !disconnect_ok) return false;

  uint8_t local_mac[ESP_NOW_ETH_ALEN] = {};
  if (!checkEsp("get_station_mac",
                esp_wifi_get_mac(WIFI_IF_STA, local_mac))) {
    return false;
  }
  Serial.print("LOCAL_STA_MAC role=gateway mac=");
  printMac(local_mac);
  Serial.println();

  bool ok = true;
  ok &= checkEsp("set_channel",
                 esp_wifi_set_channel(kTestWifiChannel, WIFI_SECOND_CHAN_NONE));
  ok &= checkEsp("enable_lr_protocol",
                 esp_wifi_set_protocol(
                     WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G |
                                       WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR));
  ok &= checkEsp("set_tx_power_19_5_dbm", esp_wifi_set_max_tx_power(78));
  ok &= checkEsp("esp_now_init", esp_now_init());
  if (!ok) {
    Serial.println("RADIO_INIT status=FAILED critical_configuration_error=1");
    return false;
  }
  if (!checkEsp("set_espnow_rate_250kbps",
                esp_wifi_config_espnow_rate(WIFI_IF_STA,
                                           WIFI_PHY_RATE_LORA_250K)) ||
      !checkEsp("register_receive_callback",
                esp_now_register_recv_cb(onDataReceived))) {
    return false;
  }

  Serial.printf("RADIO_READY channel=%u phy_rate_kbps=250 tx_power_dbm=19.5\n",
                kTestWifiChannel);
  Serial.println("RSSI status=UNAVAILABLE reason=idf_4_4_receive_callback_has_no_rx_metadata");

  if (!isPeerMacConfigured(kSensorPeerMac)) {
    Serial.println("PEER_CONFIG status=UNSET role=sensor action=set_kSensorPeerMac_from_sensor_LOCAL_STA_MAC");
    return false;
  }

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, kSensorPeerMac, ESP_NOW_ETH_ALEN);
  peer.channel = kTestWifiChannel;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  return checkEsp("add_sensor_peer", esp_now_add_peer(&peer));
}

void resetBatchState(const Packet& first_fragment) {
  batchState = {};
  batchState.active = true;
  batchState.node_id = first_fragment.node_id;
  batchState.batch_id = first_fragment.batch_id;
  batchState.expected_fragments = first_fragment.fragment_count;
}

bool sameFragment(const Packet& left, const Packet& right) {
  return left.version == right.version &&
         left.message_type == right.message_type &&
         left.node_id == right.node_id && left.batch_id == right.batch_id &&
         left.sequence_id == right.sequence_id &&
         left.fragment_index == right.fragment_index &&
         left.fragment_count == right.fragment_count &&
         left.payload_length == right.payload_length &&
         memcmp(left.payload, right.payload, left.payload_length) == 0;
}

void sendAck(const Packet& data_packet) {
  Packet ack = {};
  ack.version = BeeCoolerProtocol::kProtocolVersion;
  ack.message_type = BeeCoolerProtocol::MessageType::kAck;
  ack.node_id = data_packet.node_id;
  ack.batch_id = data_packet.batch_id;
  ack.sequence_id = nextAckSequenceId++;
  ack.fragment_index = 0U;
  ack.fragment_count = 1U;
  ack.payload_length = BeeCoolerIntegrationTest::kAckPayloadSize;
  const BeeCoolerIntegrationTest::AckPayload payload = {
      data_packet.sequence_id, data_packet.fragment_index};
  BeeCoolerIntegrationTest::serializeAck(payload, ack.payload);

  uint8_t wire[BeeCoolerProtocol::kMaxSerializedPacketSize] = {};
  size_t wire_size = 0U;
  const Status serialize_status = BeeCoolerProtocol::serialize(
      ack, wire, sizeof(wire), wire_size);
  if (serialize_status != Status::kOk) {
    Serial.printf("ACK_TX batch_id=%lu acked_sequence_id=%lu fragment=%u result=SERIALIZE_ERROR status=%u\n",
                  static_cast<unsigned long>(data_packet.batch_id),
                  static_cast<unsigned long>(data_packet.sequence_id),
                  data_packet.fragment_index,
                  static_cast<unsigned>(serialize_status));
    return;
  }
  const esp_err_t result = esp_now_send(kSensorPeerMac, wire, wire_size);
  if (result == ESP_OK) ++statistics.acks_transmitted;
  Serial.printf("ACK_TX batch_id=%lu ack_sequence_id=%lu acked_sequence_id=%lu fragment=%u result=%s bytes=%u\n",
                static_cast<unsigned long>(data_packet.batch_id),
                static_cast<unsigned long>(ack.sequence_id),
                static_cast<unsigned long>(data_packet.sequence_id),
                data_packet.fragment_index, esp_err_to_name(result),
                static_cast<unsigned>(wire_size));
}

void printSample(uint32_t batch_id, uint32_t batch_start_ms,
                 const BeeCoolerIntegrationTest::Sample& sample) {
  const uint32_t sensor_ms =
      batch_start_ms + static_cast<uint32_t>(sample.elapsed_ms);
  Serial.printf("RECORD batch_id=%lu index=%u sensor_ms=%lu elapsed_ms=%u internal_temp_c=%.2f internal_rh_pct=%.2f external_temp_c=%.2f external_rh_pct=%.2f accel_x_g=%.4f accel_y_g=%.4f accel_z_g=%.4f accel_magnitude_g=%.4f\n",
                static_cast<unsigned long>(batch_id), sample.sample_index,
                static_cast<unsigned long>(sensor_ms), sample.elapsed_ms,
                sample.internal_temperature_c,
                sample.internal_humidity_percent,
                sample.external_temperature_c,
                sample.external_humidity_percent, sample.acceleration_x_g,
                sample.acceleration_y_g, sample.acceleration_z_g,
                sample.acceleration_magnitude_g);
}

void completeBatch() {
  uint8_t batch_bytes[kMaximumBatchBytes] = {};
  size_t batch_size = 0U;
  const Status status = BeeCoolerProtocol::reassembleBatch(
      batchState.fragments, batchState.expected_fragments, batch_bytes,
      sizeof(batch_bytes), batch_size);
  if (status != Status::kOk) {
    ++statistics.malformed_packets;
    Serial.printf("BATCH_REASSEMBLY_ERROR batch_id=%lu status=%u\n",
                  static_cast<unsigned long>(batchState.batch_id),
                  static_cast<unsigned>(status));
    batchState = {};
    return;
  }

  BeeCoolerIntegrationTest::BatchHeader header = {};
  if (!BeeCoolerIntegrationTest::deserializeBatchHeader(
          batch_bytes, batch_size, header)) {
    ++statistics.malformed_packets;
    Serial.printf("BATCH_SCHEMA_ERROR batch_id=%lu bytes=%u\n",
                  static_cast<unsigned long>(batchState.batch_id),
                  static_cast<unsigned>(batch_size));
    batchState = {};
    return;
  }

  ++statistics.completed_batches;
  Serial.printf("BATCH_COMPLETE batch_id=%lu batch_start_ms=%lu bytes=%u records=%u interval_ms=%u duplicates=%u\n",
                static_cast<unsigned long>(batchState.batch_id),
                static_cast<unsigned long>(header.batch_start_ms),
                static_cast<unsigned>(batch_size), header.record_count,
                header.nominal_sample_interval_ms, batchState.duplicate_count);
  for (uint16_t i = 0U; i < header.record_count; ++i) {
    BeeCoolerIntegrationTest::Sample sample = {};
    const uint8_t* record = batch_bytes +
                            BeeCoolerIntegrationTest::kBatchHeaderSize +
                            static_cast<size_t>(i) *
                                BeeCoolerIntegrationTest::kRecordWireSize;
    if (!BeeCoolerIntegrationTest::deserializeRecord(
            record, BeeCoolerIntegrationTest::kRecordWireSize, sample)) {
      ++statistics.malformed_packets;
      break;
    }
    printSample(batchState.batch_id, header.batch_start_ms, sample);
  }
  statistics.receive_queue_drops = callbackQueueDrops;
  Serial.printf("GATEWAY_STATS valid_data_packets=%lu unique_fragments=%lu duplicate_fragments=%lu malformed_packets=%lu crc_failures=%lu acks_transmitted=%lu completed_batches=%lu incomplete_batches_replaced=%lu sequence_gaps=%lu receive_queue_drops=%lu\n",
                static_cast<unsigned long>(statistics.valid_data_packets),
                static_cast<unsigned long>(statistics.unique_fragments),
                static_cast<unsigned long>(statistics.duplicate_fragments),
                static_cast<unsigned long>(statistics.malformed_packets),
                static_cast<unsigned long>(statistics.crc_failures),
                static_cast<unsigned long>(statistics.acks_transmitted),
                static_cast<unsigned long>(statistics.completed_batches),
                static_cast<unsigned long>(statistics.incomplete_batches_replaced),
                static_cast<unsigned long>(statistics.sequence_gaps),
                static_cast<unsigned long>(statistics.receive_queue_drops));
  lastCompletedBatch = batchState;
  batchState = {};
}

void observeSequence(uint32_t sequence_id) {
  if (haveLastUniqueSequence && sequence_id > lastUniqueSequence + 1U) {
    statistics.sequence_gaps += sequence_id - lastUniqueSequence - 1U;
    Serial.printf("SEQUENCE_GAP previous=%lu current=%lu missing=%lu\n",
                  static_cast<unsigned long>(lastUniqueSequence),
                  static_cast<unsigned long>(sequence_id),
                  static_cast<unsigned long>(sequence_id - lastUniqueSequence - 1U));
  }
  if (!haveLastUniqueSequence || sequence_id > lastUniqueSequence) {
    lastUniqueSequence = sequence_id;
    haveLastUniqueSequence = true;
  }
}

void processFrame(const ReceivedFrame& frame) {
  if (memcmp(frame.sender_mac, kSensorPeerMac, ESP_NOW_ETH_ALEN) != 0) {
    ++statistics.malformed_packets;
    return;
  }

  Packet packet = {};
  const Status status =
      BeeCoolerProtocol::deserialize(frame.bytes, frame.length, packet);
  if (status != Status::kOk) {
    ++statistics.malformed_packets;
    if (status == Status::kCrcMismatch) ++statistics.crc_failures;
    Serial.printf("PACKET_REJECT status=%u bytes=%u malformed=%lu crc_failures=%lu\n",
                  static_cast<unsigned>(status), frame.length,
                  static_cast<unsigned long>(statistics.malformed_packets),
                  static_cast<unsigned long>(statistics.crc_failures));
    return;
  }
  if (packet.message_type !=
      BeeCoolerProtocol::MessageType::kTelemetryBatchFragment) {
    ++statistics.malformed_packets;
    Serial.printf("PACKET_REJECT status=unexpected_message_type type=%u\n",
                  static_cast<unsigned>(packet.message_type));
    return;
  }
  ++statistics.valid_data_packets;

  if (packet.fragment_count > kMaximumBatchFragments) {
    ++statistics.malformed_packets;
    Serial.printf("PACKET_REJECT batch_id=%lu reason=fragment_count_exceeds_test_limit count=%u limit=%u\n",
                  static_cast<unsigned long>(packet.batch_id),
                  packet.fragment_count,
                  static_cast<unsigned>(kMaximumBatchFragments));
    return;
  }


  if (lastCompletedBatch.active &&
      lastCompletedBatch.node_id == packet.node_id &&
      lastCompletedBatch.batch_id == packet.batch_id &&
      packet.fragment_count == lastCompletedBatch.expected_fragments &&
      lastCompletedBatch.present[packet.fragment_index]) {
    if (!sameFragment(lastCompletedBatch.fragments[packet.fragment_index],
                      packet)) {
      ++statistics.malformed_packets;
      Serial.printf("PACKET_REJECT batch_id=%lu fragment=%u reason=conflicts_with_completed_fragment\n",
                    static_cast<unsigned long>(packet.batch_id),
                    packet.fragment_index);
      return;
    }
    ++statistics.duplicate_fragments;
    Serial.printf("FRAGMENT_RX batch_id=%lu fragment=%u/%u sequence_id=%lu result=DUPLICATE_COMPLETED\n",
                  static_cast<unsigned long>(packet.batch_id),
                  packet.fragment_index, packet.fragment_count,
                  static_cast<unsigned long>(packet.sequence_id));
    sendAck(packet);
    return;
  }

  if (!batchState.active || batchState.batch_id != packet.batch_id ||
      batchState.node_id != packet.node_id) {
    if (batchState.active &&
        batchState.received_fragments < batchState.expected_fragments) {
      ++statistics.incomplete_batches_replaced;
      Serial.printf("BATCH_REPLACED old_batch_id=%lu received=%u expected=%u new_batch_id=%lu\n",
                    static_cast<unsigned long>(batchState.batch_id),
                    batchState.received_fragments,
                    batchState.expected_fragments,
                    static_cast<unsigned long>(packet.batch_id));
    }
    resetBatchState(packet);
  }

  if (batchState.expected_fragments != packet.fragment_count) {
    ++statistics.malformed_packets;
    Serial.printf("PACKET_REJECT batch_id=%lu reason=fragment_count_changed\n",
                  static_cast<unsigned long>(packet.batch_id));
    return;
  }

  const uint16_t index = packet.fragment_index;
  if (batchState.present[index]) {
    if (!sameFragment(batchState.fragments[index], packet)) {
      ++statistics.malformed_packets;
      Serial.printf("PACKET_REJECT batch_id=%lu fragment=%u reason=conflicting_duplicate\n",
                    static_cast<unsigned long>(packet.batch_id), index);
      return;
    }
    ++statistics.duplicate_fragments;
    ++batchState.duplicate_count;
    Serial.printf("FRAGMENT_RX batch_id=%lu fragment=%u/%u sequence_id=%lu result=DUPLICATE\n",
                  static_cast<unsigned long>(packet.batch_id), index,
                  packet.fragment_count,
                  static_cast<unsigned long>(packet.sequence_id));
    sendAck(packet);
    return;
  }

  batchState.fragments[index] = packet;
  batchState.present[index] = true;
  ++batchState.received_fragments;
  ++statistics.unique_fragments;
  observeSequence(packet.sequence_id);
  Serial.printf("FRAGMENT_RX batch_id=%lu fragment=%u/%u sequence_id=%lu result=UNIQUE received=%u\n",
                static_cast<unsigned long>(packet.batch_id), index,
                packet.fragment_count,
                static_cast<unsigned long>(packet.sequence_id),
                batchState.received_fragments);
  sendAck(packet);
  if (batchState.received_fragments == batchState.expected_fragments) {
    completeBatch();
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000U);
  Serial.println("BOOT role=gateway_espnow_test cloud_enabled=0");
  receiveQueue = xQueueCreate(kReceiveQueueDepth, sizeof(ReceivedFrame));
  if (receiveQueue == nullptr) {
    Serial.println("INIT_ERROR component=receive_queue");
    return;
  }
  radioReady = initializeRadio();
}

void loop() {
  if (!radioReady) {
    delay(1000U);
    return;
  }
  ReceivedFrame frame = {};
  if (xQueueReceive(receiveQueue, &frame, pdMS_TO_TICKS(100U)) == pdTRUE) {
    processFrame(frame);
  }
}
