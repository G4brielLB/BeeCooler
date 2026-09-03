#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <BeeCoolerIntegrationTest.h>
#include <BeeCoolerProtocol.h>
#include <BeeCoolerSensors.h>

#include <string.h>

namespace {

using BeeCoolerIntegrationTest::AckPayload;
using BeeCoolerIntegrationTest::Sample;
using BeeCoolerProtocol::Packet;
using BeeCoolerProtocol::Status;

// Replace this with the station MAC printed by gateway_espnow_test.
constexpr uint8_t kGatewayPeerMac[ESP_NOW_ETH_ALEN] = {0, 0, 0, 0, 0, 0};
constexpr uint8_t kTestWifiChannel = 6U;
constexpr uint32_t kSensorNodeId = 1U;
constexpr uint32_t kAckTimeoutMs = 300U;
constexpr uint32_t kRetryBackoffMs = 50U;
constexpr uint8_t kMaximumAttempts = 5U;
constexpr size_t kExpectedFragmentCount =
    (BeeCoolerIntegrationTest::kNormalBatchWireSize +
     BeeCoolerProtocol::kMaxPayloadSize - 1U) /
    BeeCoolerProtocol::kMaxPayloadSize;
constexpr size_t kReceiveQueueDepth = 8U;

static_assert(kExpectedFragmentCount == 7U,
              "Unexpected 60-record integration batch fragment count");

struct ReceivedFrame {
  uint8_t sender_mac[ESP_NOW_ETH_ALEN];
  uint16_t length;
  uint8_t bytes[BeeCoolerProtocol::kMaxSerializedPacketSize];
};

struct BatchDiagnostics {
  uint32_t total_attempts;
  uint16_t acknowledged_by_attempt[kMaximumAttempts];
  uint16_t failed_fragments;
};

QueueHandle_t receiveQueue = nullptr;
volatile uint32_t ackReceiveQueueDrops = 0U;
bool radioReady = false;
bool sensorsReady = false;
Sample samples[BeeCoolerIntegrationTest::kTargetRecordCount];
size_t acquiredRecordCount = 0U;
uint32_t batchStartMs = 0U;
uint32_t nextSampleMs = 0U;
uint32_t nextBatchId = 1U;
uint32_t nextPacketSequenceId = 1U;

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
    return;
  }
  ReceivedFrame frame = {};
  memcpy(frame.sender_mac, mac, ESP_NOW_ETH_ALEN);
  frame.length = static_cast<uint16_t>(length);
  memcpy(frame.bytes, data, static_cast<size_t>(length));
  if (xQueueSend(receiveQueue, &frame, 0U) != pdTRUE &&
      ackReceiveQueueDrops < UINT32_MAX) {
    ++ackReceiveQueueDrops;
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
  Serial.print("LOCAL_STA_MAC role=sensor mac=");
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

  if (!isPeerMacConfigured(kGatewayPeerMac)) {
    Serial.println("PEER_CONFIG status=UNSET role=gateway action=set_kGatewayPeerMac_from_gateway_LOCAL_STA_MAC");
    return false;
  }

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, kGatewayPeerMac, ESP_NOW_ETH_ALEN);
  peer.channel = kTestWifiChannel;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  if (!checkEsp("add_gateway_peer", esp_now_add_peer(&peer))) {
    return false;
  }
  return true;
}

void beginCollection() {
  acquiredRecordCount = 0U;
  batchStartMs = millis();
  nextSampleMs = batchStartMs;
  Serial.printf("BATCH_COLLECTION_START batch_id=%lu target_records=%u interval_ms=%u test_only=1\n",
                static_cast<unsigned long>(nextBatchId),
                static_cast<unsigned>(BeeCoolerIntegrationTest::kTargetRecordCount),
                BeeCoolerIntegrationTest::kNominalSampleIntervalMs);
}

bool matchingAckAvailable(uint32_t batch_id, uint32_t sequence_id,
                          uint16_t fragment_index) {
  ReceivedFrame frame = {};
  while (xQueueReceive(receiveQueue, &frame, 0U) == pdTRUE) {
    Packet packet = {};
    if (memcmp(frame.sender_mac, kGatewayPeerMac, ESP_NOW_ETH_ALEN) != 0 ||
        BeeCoolerProtocol::deserialize(frame.bytes, frame.length, packet) !=
            Status::kOk ||
        packet.message_type != BeeCoolerProtocol::MessageType::kAck ||
        packet.batch_id != batch_id) {
      continue;
    }
    AckPayload ack = {};
    if (BeeCoolerIntegrationTest::deserializeAck(
            packet.payload, packet.payload_length, ack) &&
        ack.acked_sequence_id == sequence_id &&
        ack.acked_fragment_index == fragment_index) {
      return true;
    }
  }
  return false;
}

bool transmitFragment(const Packet& fragment, uint8_t* wire,
                      BatchDiagnostics& diagnostics) {
  size_t wire_size = 0U;
  if (BeeCoolerProtocol::serialize(fragment, wire,
                                   BeeCoolerProtocol::kMaxSerializedPacketSize,
                                   wire_size) != Status::kOk) {
    Serial.printf("FRAGMENT_ERROR batch_id=%lu fragment=%u reason=serialize_failed\n",
                  static_cast<unsigned long>(fragment.batch_id),
                  fragment.fragment_index);
    ++diagnostics.failed_fragments;
    return false;
  }

  ReceivedFrame stale_frame = {};
  while (xQueueReceive(receiveQueue, &stale_frame, 0U) == pdTRUE) {
  }

  for (uint8_t attempt = 1U; attempt <= kMaximumAttempts; ++attempt) {
    ++diagnostics.total_attempts;
    const uint32_t sent_at_ms = millis();
    const esp_err_t send_result =
        esp_now_send(kGatewayPeerMac, wire, wire_size);
    Serial.printf("FRAGMENT_TX batch_id=%lu fragment=%u/%u sequence_id=%lu attempt=%u send_result=%s bytes=%u\n",
                  static_cast<unsigned long>(fragment.batch_id),
                  fragment.fragment_index,
                  static_cast<unsigned>(fragment.fragment_count),
                  static_cast<unsigned long>(fragment.sequence_id), attempt,
                  esp_err_to_name(send_result), static_cast<unsigned>(wire_size));

    if (send_result == ESP_OK) {
      while (static_cast<uint32_t>(millis() - sent_at_ms) < kAckTimeoutMs) {
        if (matchingAckAvailable(fragment.batch_id, fragment.sequence_id,
                                 fragment.fragment_index)) {
          const uint32_t rtt_ms = millis() - sent_at_ms;
          ++diagnostics.acknowledged_by_attempt[attempt - 1U];
          Serial.printf("FRAGMENT_RESULT batch_id=%lu fragment=%u sequence_id=%lu attempt=%u result=ACK rtt_ms=%lu\n",
                        static_cast<unsigned long>(fragment.batch_id),
                        fragment.fragment_index,
                        static_cast<unsigned long>(fragment.sequence_id),
                        attempt, static_cast<unsigned long>(rtt_ms));
          return true;
        }
        delay(1U);
      }
    }
    Serial.printf("FRAGMENT_RESULT batch_id=%lu fragment=%u sequence_id=%lu attempt=%u result=TIMEOUT\n",
                  static_cast<unsigned long>(fragment.batch_id),
                  fragment.fragment_index,
                  static_cast<unsigned long>(fragment.sequence_id), attempt);
    if (attempt < kMaximumAttempts) delay(kRetryBackoffMs);
  }
  ++diagnostics.failed_fragments;
  return false;
}

void transmitCollectedBatch() {
  uint8_t batch_bytes[BeeCoolerIntegrationTest::kNormalBatchWireSize] = {};
  size_t batch_size = 0U;
  if (!BeeCoolerIntegrationTest::serializeBatch(
          samples, acquiredRecordCount,
          BeeCoolerIntegrationTest::kNominalSampleIntervalMs, batch_bytes,
          sizeof(batch_bytes), batch_size)) {
    Serial.printf("BATCH_ERROR batch_id=%lu reason=test_schema_serialization_failed\n",
                  static_cast<unsigned long>(nextBatchId));
    return;
  }

  Packet fragments[kExpectedFragmentCount] = {};
  size_t fragment_count = 0U;
  const BeeCoolerProtocol::FragmentationOptions options = {
      BeeCoolerProtocol::MessageType::kTelemetryBatchFragment, kSensorNodeId,
      nextBatchId, nextPacketSequenceId};
  const Status status = BeeCoolerProtocol::fragmentBatch(
      batch_bytes, batch_size, options, fragments, kExpectedFragmentCount,
      fragment_count);
  if (status != Status::kOk) {
    Serial.printf("BATCH_ERROR batch_id=%lu reason=fragmentation_failed status=%u\n",
                  static_cast<unsigned long>(nextBatchId),
                  static_cast<unsigned>(status));
    return;
  }

  Serial.printf("BATCH_TX_START batch_id=%lu records=%u logical_bytes=%u fragments=%u\n",
                static_cast<unsigned long>(nextBatchId),
                static_cast<unsigned>(acquiredRecordCount),
                static_cast<unsigned>(batch_size),
                static_cast<unsigned>(fragment_count));
  BatchDiagnostics diagnostics = {};
  uint8_t wire[BeeCoolerProtocol::kMaxSerializedPacketSize] = {};
  const uint32_t transmission_start_ms = millis();
  for (size_t i = 0U; i < fragment_count; ++i) {
    transmitFragment(fragments[i], wire, diagnostics);
  }
  const uint32_t duration_ms = millis() - transmission_start_ms;
  Serial.printf("BATCH_TX_SUMMARY batch_id=%lu records=%u logical_bytes=%u fragments=%u attempts=%lu ack_attempt_1=%u ack_attempt_2=%u ack_attempt_3=%u ack_attempt_4=%u ack_attempt_5=%u failed_fragments=%u duration_ms=%lu complete_radio_delivery=%u ack_receive_queue_drops=%lu\n",
                static_cast<unsigned long>(nextBatchId),
                static_cast<unsigned>(acquiredRecordCount),
                static_cast<unsigned>(batch_size),
                static_cast<unsigned>(fragment_count),
                static_cast<unsigned long>(diagnostics.total_attempts),
                diagnostics.acknowledged_by_attempt[0],
                diagnostics.acknowledged_by_attempt[1],
                diagnostics.acknowledged_by_attempt[2],
                diagnostics.acknowledged_by_attempt[3],
                diagnostics.acknowledged_by_attempt[4],
                diagnostics.failed_fragments,
                static_cast<unsigned long>(duration_ms),
                diagnostics.failed_fragments == 0U ? 1U : 0U,
                static_cast<unsigned long>(ackReceiveQueueDrops));
  Serial.println("BATCH_DISCARD policy=integration_test_only persistent_commit_not_implemented=1");
  nextPacketSequenceId += static_cast<uint32_t>(fragment_count);
  ++nextBatchId;
}

void acquireDueSample() {
  const BeeCoolerSensors::SensorReading reading = BeeCoolerSensors::read();
  Sample& sample = samples[acquiredRecordCount];
  sample = {static_cast<uint16_t>(acquiredRecordCount),
            static_cast<uint16_t>(millis() - batchStartMs),
            reading.internalTemperatureC,
            reading.internalHumidityPercent,
            reading.externalTemperatureC,
            reading.externalHumidityPercent,
            reading.accelerationXG,
            reading.accelerationYG,
            reading.accelerationZG,
            reading.accelerationMagnitudeG};
  ++acquiredRecordCount;
  Serial.printf("SAMPLE batch_id=%lu index=%u elapsed_ms=%u\n",
                static_cast<unsigned long>(nextBatchId), sample.sample_index,
                sample.elapsed_ms);
  nextSampleMs = batchStartMs +
                 static_cast<uint32_t>(acquiredRecordCount) *
                     BeeCoolerIntegrationTest::kNominalSampleIntervalMs;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000U);
  Serial.println("BOOT role=sensor_espnow_test final_sampling_policy=0");

  receiveQueue = xQueueCreate(kReceiveQueueDepth, sizeof(ReceivedFrame));
  if (receiveQueue == nullptr) {
    Serial.println("INIT_ERROR component=receive_queue");
    return;
  }

  const BeeCoolerSensors::SensorStatus sensor_status =
      BeeCoolerSensors::begin(Serial);
  sensorsReady = sensor_status.internalSht30Detected &&
                 sensor_status.externalSht30Detected &&
                 sensor_status.adxl345Detected;
  Serial.printf("SENSOR_INIT internal_sht30=%u external_sht30=%u adxl345=%u ready=%u\n",
                sensor_status.internalSht30Detected,
                sensor_status.externalSht30Detected,
                sensor_status.adxl345Detected, sensorsReady);
  radioReady = initializeRadio();
  if (sensorsReady && radioReady) beginCollection();
}

void loop() {
  if (!sensorsReady || !radioReady) {
    delay(1000U);
    return;
  }
  if (acquiredRecordCount < BeeCoolerIntegrationTest::kTargetRecordCount &&
      static_cast<int32_t>(millis() - nextSampleMs) >= 0) {
    acquireDueSample();
  }
  if (acquiredRecordCount == BeeCoolerIntegrationTest::kTargetRecordCount) {
    transmitCollectedBatch();
    beginCollection();
  }
  delay(1U);
}
