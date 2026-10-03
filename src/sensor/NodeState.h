#pragma once

#include <Arduino.h>

#include <BeeCoolerLogic.h>

// State kept in RTC slow memory across deep sleep (Arquitetura v1.2, 5.4).
// It does not survive a power loss; the DS1302 RAM block and the flash ring
// are the durable copies (UC-10).
struct NodeState {
  uint32_t magic;
  uint32_t last_seq;        // last seq written to the ring (0 = none)
  uint32_t confirmed_seq;   // highest seq acked by the gateway
  uint32_t last_sync_epoch;
  uint32_t last_lora_slot;  // last slot in which a session ran
  uint32_t cycles_since_sd_refresh;
  uint16_t boot_count;
  uint16_t cycles_since_lora;  // used only while the DS1302 has no valid time
  uint16_t frame_seq;       // identity of the next LoRa frame
  uint16_t sd_free_mb;
  uint16_t vbat_tx_mv;      // battery measured while transmitting
  uint16_t vbat_history[3];
  uint8_t vbat_history_count;
  uint8_t sd_errors;        // saturating
  uint8_t failed_sessions;  // consecutive
  uint8_t last_ack_rssi;    // -dBm of the last received ACK
  uint8_t pending_flags_lo; // record flags to carry into the next record
  uint8_t pending_flags_hi;
  BeeCoolerLogic::PowerState power;
  BeeCoolerLogic::StuckHistory stuck;
};

constexpr uint32_t kNodeStateMagic = 0xBEE0C001U;

extern NodeState gState;

// True when the state was valid (deep-sleep wake); otherwise it is zeroed.
bool nodeStateInit();

inline uint16_t pendingFlags() {
  return static_cast<uint16_t>(gState.pending_flags_lo |
                               (gState.pending_flags_hi << 8));
}
inline void setPendingFlags(uint16_t flags) {
  gState.pending_flags_lo = static_cast<uint8_t>(flags);
  gState.pending_flags_hi = static_cast<uint8_t>(flags >> 8);
}
