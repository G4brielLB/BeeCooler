#pragma once

#include <Arduino.h>

// task_radio (Nuvem 4.1): the only task that touches the LoRa UART. It
// validates, de-duplicates, persists and only then sends the ACK. It never
// waits for Wi-Fi, DNS or TLS: the ACK does not depend on the cloud.
namespace RadioTask {

struct Stats {
  uint32_t frames_ok;
  uint32_t crc_failures;     // from the stream parser
  uint32_t discarded_bytes;
  uint32_t duplicate_records;
  uint32_t gap_frames;       // DATA ahead of acked_up_to + 1: not stored
  uint32_t records_stored;
  uint32_t status_stored;
  uint32_t hello_seen;
  uint32_t acks_sent;
  uint32_t persist_failures;
  uint32_t last_activity_ms; // millis() of the last ACK sent
};

void start();
Stats stats();

// millis() of the last frame handled (the uplink waits for the session to end).
uint32_t lastActivityMs();

#ifdef BEE_GATEWAY_FAULT_INJECTION
// CT-03 hooks, set from the serial console (`fault ...`).
void faultCorruptEvery(uint32_t n);   // corrupt 1 of every n received frames
void faultDropAcks(uint32_t count);   // swallow the next `count` ACKs
void faultRebootAfterReceive(bool on);// restart between reception and persistence
#endif

}  // namespace RadioTask
