#include "LoraSession.h"

#include <BeeCoolerLogic.h>

#include "BatteryMonitor.h"
#include "NodeState.h"
#include "config.h"

using namespace BeeCoolerLink;
using namespace BeeCoolerRecord;

namespace {

uint32_t buildEpoch() {
  static uint32_t cached = 0;
  if (cached == 0) {
    BeeCoolerLogic::parseBuildEpoch(__DATE__, __TIME__, cached);
  }
  return cached;
}

// Runs while the radio transmits: the battery under load (8.2).
void sampleVbatUnderLoad(void*) { gState.vbat_tx_mv = BatteryMonitor::readMv(); }

}  // namespace

Header LoraSession::makeHeader(FrameType type, uint32_t first_seq,
                               uint8_t n_rec) {
  Header h = {};
  h.type = type;
  h.node_id = node_id_;
  h.boot_id = static_cast<uint8_t>(gState.boot_count & 0xFF);
  h.frame_seq = gState.frame_seq;
  h.attempt = 1;
  h.first_seq = first_seq;
  h.n_rec = n_rec;
  return h;
}

// UC-06: reject absurd times, correct the DS1302 when it is off by > 2 s.
bool LoraSession::applyEpoch(uint32_t epoch) {
  if (epoch == 0 || epoch < buildEpoch()) return false;  // gateway without NTP / absurd
  uint32_t local = 0;
  const bool local_valid = clock_.readEpoch(local) && local >= buildEpoch();
  if (local_valid) {
    const uint32_t diff = epoch > local ? epoch - local : local - epoch;
    if (diff > kClockMaxJumpS) return false;
    if (diff <= kClockToleranceS) {
      gState.last_sync_epoch = epoch;
      return true;
    }
  }
  clock_.writeEpoch(epoch);
  gState.last_sync_epoch = epoch;
  clock_adjusted_ = true;
  return true;
}

// One frame with up to kMaxAttempts tries. The ACK timer starts when AUX falls
// (end of TX), not when the UART write returns (6.6).
bool LoraSession::exchange(uint8_t* frame, size_t size, uint16_t frame_seq,
                           Ack& ack, SessionResult* stats, bool sample_vbat) {
  for (uint8_t attempt = 1; attempt <= BeeCoolerLink::kMaxAttempts; ++attempt) {
    if (setAttempt(frame, size, attempt) != Result::kOk) return false;

    lora_.flushInput();
    parser_.reset();
    uint32_t tx_end_us = 0;
    const bool sent = lora_.send(frame, size, tx_end_us, 2000,
                                 (sample_vbat && attempt == 1)
                                     ? sampleVbatUnderLoad
                                     : nullptr,
                                 nullptr);
    if (sent) {
      while ((micros() - tx_end_us) < kAckTimeoutMs * 1000UL) {
        while (lora_.available() > 0) {
          bool got = parser_.feed(static_cast<uint8_t>(lora_.read()));
          while (got) {
            Frame decoded;
            if (decodeFrame(parser_.frame(), parser_.frameSize(), decoded) ==
                    Result::kOk &&
                decoded.is_ack && decoded.ack.node_id == node_id_ &&
                decoded.ack.frame_seq == frame_seq) {
              const uint32_t rtt = micros() - tx_end_us;
              ack = decoded.ack;
              if (stats != nullptr) {
                if (stats->min_rtt_us == 0 || rtt < stats->min_rtt_us) stats->min_rtt_us = rtt;
                if (rtt > stats->max_rtt_us) stats->max_rtt_us = rtt;
              }
              return true;
            }
            got = parser_.poll();
          }
        }
        delay(1);
      }
    }
    if (stats != nullptr && attempt > 1) ++stats->retransmissions;
    if (attempt < BeeCoolerLink::kMaxAttempts) delay(random(kBackoffMinMs, kBackoffMaxMs + 1));
  }
  return false;
}

bool LoraSession::hello(uint8_t reset_reason, bool rtc_valid,
                        bool& clock_adjusted) {
  clock_adjusted_ = false;
  clock_adjusted = false;
  if (!lora_.setMode(BeeCoolerLora::Mode::kNormal)) return false;

  Hello h = {kFirmwareVersion, reset_reason,
             static_cast<uint8_t>(rtc_valid ? 1 : 0), gState.last_seq,
             gState.boot_count};
  uint8_t frame[kMaxFrameSize];
  size_t size = 0;
  const uint16_t frame_seq = gState.frame_seq;
  encodeHello(makeHeader(FrameType::kHello, gState.last_seq, 0), h, frame,
              sizeof(frame), size);

  Ack ack = {};
  const bool ok = exchange(frame, size, frame_seq, ack, nullptr, false);
  ++gState.frame_seq;
  if (ok) {
    gState.confirmed_seq = ack.acked_up_to;
    gState.last_ack_rssi = ack.rssi;
    applyEpoch(ack.epoch);
    clock_adjusted = clock_adjusted_;
  }
  lora_.setMode(BeeCoolerLora::Mode::kSleep);
  return ok;
}

SessionResult LoraSession::run(const RecordV1& latest, bool status_only,
                               const SessionInfo& info) {
  SessionResult result = {};
  clock_adjusted_ = false;

  if (!lora_.setMode(BeeCoolerLora::Mode::kNormal)) {
    ++gState.failed_sessions;
    return result;
  }

  // ---- STATUS (one per session; carries the freshest record)
  {
    Status s = {};
    s.record = latest;
    s.backlog = static_cast<uint16_t>(
        min<uint32_t>(0xFFFF, gState.last_seq - min(gState.confirmed_seq, gState.last_seq)));
    s.sd_free_mb = info.sd_free_mb;
    s.sd_errors = info.sd_errors;
    s.reset_reason = info.reset_reason;
    s.rssi = gState.last_ack_rssi;
    s.failed_sessions = gState.failed_sessions;
    s.vbat_tx_mv = gState.vbat_tx_mv;
    s.fw_version = kFirmwareVersion;

    uint8_t frame[kMaxFrameSize];
    size_t size = 0;
    const uint16_t frame_seq = gState.frame_seq;
    encodeStatus(makeHeader(FrameType::kStatus, gState.last_seq, 1), s, frame,
                 sizeof(frame), size);

    Ack ack = {};
    const bool ok = exchange(frame, size, frame_seq, ack, &result, true);
    ++gState.frame_seq;
    if (!ok) {  // abort: the rest would only waste airtime (6.6)
      if (gState.failed_sessions < 255) ++gState.failed_sessions;
      lora_.setMode(BeeCoolerLora::Mode::kSleep);
      return result;
    }
    result.status_acked = true;
    result.acked_up_to = ack.acked_up_to;
    gState.confirmed_seq = ack.acked_up_to;  // the gateway is the authority
    gState.last_ack_rssi = ack.rssi;
    gState.failed_sessions = 0;
    applyEpoch(ack.epoch);
  }

  // ---- DATA from acked_up_to + 1, oldest first
  bool aborted = false;
  if (!status_only) {
    uint32_t next = max(result.acked_up_to + 1, ring_.oldestSeq(gState.last_seq));
    while (next <= gState.last_seq && result.frames_sent < kMaxDataFramesPerSession) {
      const uint8_t n = static_cast<uint8_t>(
          min<uint32_t>(kMaxRecordsPerFrame, gState.last_seq - next + 1));
      RecordV1 records[kMaxRecordsPerFrame];
      for (uint8_t i = 0; i < n; ++i) {
        if (!ring_.read(next + i, records[i])) {
          // Keep the sequence contiguous for the gateway: an unreadable slot
          // goes out as an empty record instead of stalling the backlog.
          initInvalid(records[i]);
        }
      }

      uint8_t frame[kMaxFrameSize];
      size_t size = 0;
      const uint16_t frame_seq = gState.frame_seq;
      encodeData(makeHeader(FrameType::kData, next, n), records, n, frame,
                 sizeof(frame), size);
      Ack ack = {};
      const bool ok = exchange(frame, size, frame_seq, ack, &result, false);
      ++gState.frame_seq;
      if (!ok || ack.acked_up_to < next + n - 1U) {
        aborted = true;
        break;
      }
      ++result.frames_sent;
      result.acked_up_to = ack.acked_up_to;
      gState.confirmed_seq = ack.acked_up_to;
      next = ack.acked_up_to + 1;
    }
  }

  result.completed = !aborted && (status_only || result.acked_up_to >= gState.last_seq);
  result.clock_adjusted = clock_adjusted_;
  lora_.setMode(BeeCoolerLora::Mode::kSleep);
  return result;
}
