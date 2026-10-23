#pragma once

#include <Arduino.h>

#include <BeeCoolerLink.h>
#include <BeeCoolerLora.h>
#include <BeeCoolerRecord.h>

#include "Ds1302Clock.h"
#include "FlashRing.h"

// LoRa session of the node (Arquitetura v1.2, 6.5 and 6.6, UC-04/06/07):
//   STATUS -> ACK (clock correction, acked_up_to) -> DATA from acked_up_to+1,
//   oldest first, <= 36 frames. A frame that exhausts its attempts aborts the
//   whole session; the data stays in the ring for the next one.
struct SessionInfo {
  uint16_t sd_free_mb;
  uint8_t sd_errors;
  uint8_t reset_reason;
};

struct SessionResult {
  bool status_acked;
  bool completed;         // every pending record up to the latest was acked
  uint8_t frames_sent;    // DATA frames acknowledged
  uint32_t acked_up_to;
  uint32_t min_rtt_us;
  uint32_t max_rtt_us;
  uint32_t retransmissions;
  bool clock_adjusted;
};

class LoraSession {
 public:
  LoraSession(BeeCoolerLora& lora, FlashRing& ring, Ds1302Clock& clock,
              uint8_t node_id)
      : lora_(lora), ring_(ring), clock_(clock), node_id_(node_id) {}

  // Sends HELLO after a boot and applies the epoch of the ACK. Returns true
  // when an ACK arrived.
  bool hello(uint8_t reset_reason, bool rtc_valid, bool& clock_adjusted);

  SessionResult run(const BeeCoolerRecord::RecordV1& latest, bool status_only,
                    const SessionInfo& info);

 private:
  bool exchange(uint8_t* frame, size_t size, uint16_t frame_seq,
                BeeCoolerLink::Ack& ack, SessionResult* stats, bool sample_vbat);
  bool applyEpoch(uint32_t epoch);
  BeeCoolerLink::Header makeHeader(BeeCoolerLink::FrameType type,
                                   uint32_t first_seq, uint8_t n_rec);

  BeeCoolerLora& lora_;
  FlashRing& ring_;
  Ds1302Clock& clock_;
  uint8_t node_id_;
  BeeCoolerLink::StreamParser parser_;
  bool clock_adjusted_ = false;
};
