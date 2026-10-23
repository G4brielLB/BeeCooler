#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "config.h"

// LittleFS persistence of the gateway (Arquitetura v1.2, 7.1/7.2; Nuvem 4.2):
//   /queue.bin   fixed-size items waiting for the uplink
//   /qhead.bin   number of items already consumed from the queue
//   /quar.bin    items of batches the API rejected with 400
//   /n<id>.bin   per-node state (acked_up_to, last boot_id, attempt histogram)
// Both tasks share it; every public call takes the mutex.

enum class QueueKind : uint8_t { kRecord = 1, kStatus = 2 };

struct __attribute__((packed)) QueueItem {
  uint8_t kind;      // QueueKind
  uint8_t node_id;
  uint8_t boot_id;
  uint8_t attempt;   // attempt number of the frame that carried it
  uint8_t rssi;      // -dBm at the gateway (0 = unknown)
  int8_t snr;
  uint16_t reserved;
  uint32_t seq;      // record: its seq; status: seq of the embedded record
  uint32_t rx_time;  // gateway NTP epoch at reception (0 = NTP not valid)
  uint8_t data[36];  // record: 24 B canonical record; status: 36 B STATUS
};
static_assert(sizeof(QueueItem) == kQueueItemSize, "queue item size");

struct NodeRecordState {
  uint32_t magic;
  uint8_t node_id;
  uint8_t last_boot_id;
  uint16_t last_status_frame_seq;
  uint32_t acked_up_to;          // highest contiguous seq persisted here
  uint32_t last_rx_epoch;
  uint32_t last_status_boot_key; // boot_id of the last STATUS stored
  uint8_t last_rssi;
  int8_t last_snr;
  uint16_t reserved;
  uint32_t attempt_histogram[5]; // [1..4] frames received at that attempt
};
constexpr uint32_t kNodeStateMagic = 0xBEE06A11U;

namespace GatewayStore {

bool begin();  // mounts LittleFS (formats on first use), repairs a torn tail

// ---- queue
bool appendItem(const QueueItem& item);
uint32_t pendingCount();
uint32_t quarantineCount();
// Copies up to `max` pending items starting at the head into `out`.
uint32_t peek(QueueItem* out, uint32_t max);
// Consumes `count` items from the head (after a 200) and compacts when useful.
bool consume(uint32_t count);
// Moves `count` head items to the quarantine file (after a 400).
bool quarantine(uint32_t count);

// ---- per-node state (RAM cache backed by /n<id>.bin)
NodeRecordState* node(uint8_t node_id);  // nullptr when kMaxNodes is exceeded
bool saveNode(const NodeRecordState& state);

size_t freeBytes();
SemaphoreHandle_t mutex();

}  // namespace GatewayStore
