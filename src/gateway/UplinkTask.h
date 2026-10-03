#pragma once

#include <Arduino.h>

// task_uplink (Nuvem 4.1): Wi-Fi, NTP, TLS, POST /v1/telemetry, heartbeat and
// queue cleanup. It never answers the node.
namespace Uplink {

struct Stats {
  bool wifi_connected;
  bool ntp_valid;
  bool suspended;          // 401: stop until the token changes
  uint32_t uploads_ok;
  uint32_t uploads_failed;
  uint32_t records_stored_remote;
  uint32_t duplicates_remote;
  uint32_t conflicts_remote;
  uint32_t quarantined_batches;
  int last_http_status;
  uint32_t last_free_heap_during_tls;  // P-TLS
};

void start();
Stats stats();
// Re-reads the NVS configuration (after `set ...`) and clears a 401 suspension.
void reloadConfig();

// UTC seconds when NTP is valid, otherwise 0.
uint32_t epochOrZero();

#ifdef BEE_GATEWAY_FAULT_INJECTION
void faultDropHttp200(uint32_t count);  // CT-12: pretend the 200 never arrived
#endif

}  // namespace Uplink
