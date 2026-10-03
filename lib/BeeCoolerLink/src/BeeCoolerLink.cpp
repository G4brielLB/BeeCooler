#include "BeeCoolerLink.h"

#include <string.h>

#include <BeeCoolerProtocol.h>

namespace BeeCoolerLink {

namespace {

constexpr size_t kMinFrameSize = kAckSize;  // smallest valid frame

void put16(uint8_t* p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v);
  p[1] = static_cast<uint8_t>(v >> 8);
}
void put32(uint8_t* p, uint32_t v) {
  for (uint8_t i = 0U; i < 4U; ++i) p[i] = static_cast<uint8_t>(v >> (8U * i));
}
uint16_t get16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0] | (p[1] << 8));
}
uint32_t get32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

void appendCrc(uint8_t* out, size_t body_size) {
  put32(out + body_size, BeeCoolerProtocol::generateCrc32(out, body_size));
}

bool validType(uint8_t t) {
  return t == static_cast<uint8_t>(FrameType::kData) ||
         t == static_cast<uint8_t>(FrameType::kStatus) ||
         t == static_cast<uint8_t>(FrameType::kHello) ||
         t == static_cast<uint8_t>(FrameType::kEvent) ||
         t == static_cast<uint8_t>(FrameType::kAck);
}

}  // namespace

size_t declaredLength(const uint8_t* data, size_t available) {
  if (available < 2U || data[0] != kMagic) return 0U;
  const size_t len = data[1];
  if (len < kMinFrameSize || len > kMaxFrameSize) return 0U;
  return len;
}

Result encodeFrame(const Header& header, const uint8_t* payload,
                   size_t payload_length, uint8_t* out, size_t capacity,
                   size_t& out_size) {
  if (out == nullptr || (payload == nullptr && payload_length != 0U)) {
    return Result::kBadArgument;
  }
  if (payload_length > kMaxPayload) return Result::kBadLength;
  const size_t total = kHeaderSize + payload_length + kCrcSize;
  if (capacity < total) return Result::kBufferTooSmall;

  out[0] = kMagic;
  out[1] = static_cast<uint8_t>(total);
  out[2] = static_cast<uint8_t>((kVersion << 4) |
                                (static_cast<uint8_t>(header.type) & 0x0FU));
  out[3] = header.node_id;
  out[4] = header.boot_id;
  put16(out + 5, header.frame_seq);
  out[7] = header.attempt;
  put32(out + 8, header.first_seq);
  out[12] = header.n_rec;
  if (payload_length != 0U) memcpy(out + kHeaderSize, payload, payload_length);
  appendCrc(out, kHeaderSize + payload_length);
  out_size = total;
  return Result::kOk;
}

Result encodeData(Header header, const BeeCoolerRecord::RecordV1* records,
                  uint8_t count, uint8_t* out, size_t capacity,
                  size_t& out_size) {
  if (records == nullptr || count == 0U || count > kMaxRecordsPerFrame) {
    return Result::kBadArgument;
  }
  header.type = FrameType::kData;
  header.n_rec = count;
  return encodeFrame(header, reinterpret_cast<const uint8_t*>(records),
                     count * BeeCoolerRecord::kRecordSize, out, capacity,
                     out_size);
}

Result encodeStatus(Header header, const Status& s, uint8_t* out,
                    size_t capacity, size_t& out_size) {
  uint8_t payload[kStatusPayloadSize];
  memcpy(payload, &s.record, BeeCoolerRecord::kRecordSize);
  put16(payload + 24, s.backlog);
  put16(payload + 26, s.sd_free_mb);
  payload[28] = s.sd_errors;
  payload[29] = s.reset_reason;
  payload[30] = s.rssi;
  payload[31] = s.failed_sessions;
  put16(payload + 32, s.vbat_tx_mv);
  put16(payload + 34, s.fw_version);
  header.type = FrameType::kStatus;
  header.n_rec = 1U;
  return encodeFrame(header, payload, sizeof(payload), out, capacity, out_size);
}

Result encodeHello(Header header, const Hello& h, uint8_t* out,
                   size_t capacity, size_t& out_size) {
  uint8_t payload[kHelloPayloadSize];
  put16(payload, h.fw_version);
  payload[2] = h.reset_reason;
  payload[3] = h.rtc_valid;
  put32(payload + 4, h.last_seq);
  put16(payload + 8, h.boot_count);
  header.type = FrameType::kHello;
  header.n_rec = 0U;
  return encodeFrame(header, payload, sizeof(payload), out, capacity, out_size);
}

Result encodeAck(const Ack& ack, uint8_t* out, size_t capacity,
                 size_t& out_size) {
  if (out == nullptr) return Result::kBadArgument;
  if (capacity < kAckSize) return Result::kBufferTooSmall;
  out[0] = kMagic;
  out[1] = static_cast<uint8_t>(kAckSize);
  out[2] = static_cast<uint8_t>((kVersion << 4) |
                                static_cast<uint8_t>(FrameType::kAck));
  out[3] = ack.node_id;
  put16(out + 4, ack.frame_seq);
  put32(out + 6, ack.acked_up_to);
  put32(out + 10, ack.epoch);
  out[14] = ack.rssi;
  out[15] = static_cast<uint8_t>(ack.snr);
  out[16] = ack.pending_command;
  appendCrc(out, 17U);
  out_size = kAckSize;
  return Result::kOk;
}

Result setAttempt(uint8_t* frame, size_t size, uint8_t attempt) {
  if (frame == nullptr || size < kHeaderSize + kCrcSize || frame[1] != size ||
      (frame[2] & 0x0FU) == static_cast<uint8_t>(FrameType::kAck)) {
    return Result::kBadArgument;
  }
  frame[7] = attempt;
  appendCrc(frame, size - kCrcSize);
  return Result::kOk;
}

Result decodeFrame(const uint8_t* data, size_t size, Frame& frame) {
  if (data == nullptr) return Result::kBadArgument;
  if (size < kMinFrameSize) return Result::kBadLength;
  if (data[0] != kMagic) return Result::kBadMagic;
  const size_t len = data[1];
  if (len != size || len > kMaxFrameSize) return Result::kBadLength;
  if (((data[2] >> 4) & 0x0FU) != kVersion) return Result::kBadVersion;
  const uint8_t type = data[2] & 0x0FU;
  if (!validType(type)) return Result::kBadType;
  if (get32(data + len - kCrcSize) !=
      BeeCoolerProtocol::generateCrc32(data, len - kCrcSize)) {
    return Result::kBadCrc;
  }

  memset(&frame, 0, sizeof(frame));
  if (type == static_cast<uint8_t>(FrameType::kAck)) {
    if (len != kAckSize) return Result::kBadLength;
    frame.is_ack = true;
    frame.header.type = FrameType::kAck;
    frame.header.node_id = data[3];
    frame.ack.node_id = data[3];
    frame.ack.frame_seq = get16(data + 4);
    frame.ack.acked_up_to = get32(data + 6);
    frame.ack.epoch = get32(data + 10);
    frame.ack.rssi = data[14];
    frame.ack.snr = static_cast<int8_t>(data[15]);
    frame.ack.pending_command = data[16];
    return Result::kOk;
  }

  if (len < kHeaderSize + kCrcSize) return Result::kBadLength;
  frame.is_ack = false;
  frame.header.type = static_cast<FrameType>(type);
  frame.header.node_id = data[3];
  frame.header.boot_id = data[4];
  frame.header.frame_seq = get16(data + 5);
  frame.header.attempt = data[7];
  frame.header.first_seq = get32(data + 8);
  frame.header.n_rec = data[12];
  frame.payload = data + kHeaderSize;
  frame.payload_length = len - kHeaderSize - kCrcSize;

  size_t expected = frame.payload_length;
  switch (frame.header.type) {
    case FrameType::kData:
      expected = static_cast<size_t>(frame.header.n_rec) *
                 BeeCoolerRecord::kRecordSize;
      if (frame.header.n_rec == 0U || frame.header.n_rec > kMaxRecordsPerFrame) {
        return Result::kBadPayload;
      }
      break;
    case FrameType::kStatus: expected = kStatusPayloadSize; break;
    case FrameType::kHello: expected = kHelloPayloadSize; break;
    case FrameType::kEvent: expected = kEventPayloadSize; break;
    default: break;
  }
  if (frame.payload_length != expected) return Result::kBadPayload;
  return Result::kOk;
}

Result decodeStatus(const uint8_t* p, size_t length, Status& s) {
  if (p == nullptr || length != kStatusPayloadSize) return Result::kBadPayload;
  memcpy(&s.record, p, BeeCoolerRecord::kRecordSize);
  s.backlog = get16(p + 24);
  s.sd_free_mb = get16(p + 26);
  s.sd_errors = p[28];
  s.reset_reason = p[29];
  s.rssi = p[30];
  s.failed_sessions = p[31];
  s.vbat_tx_mv = get16(p + 32);
  s.fw_version = get16(p + 34);
  return Result::kOk;
}

Result decodeHello(const uint8_t* p, size_t length, Hello& h) {
  if (p == nullptr || length != kHelloPayloadSize) return Result::kBadPayload;
  h.fw_version = get16(p);
  h.reset_reason = p[2];
  h.rtc_valid = p[3];
  h.last_seq = get32(p + 4);
  h.boot_count = get16(p + 8);
  return Result::kOk;
}

// ---------------------------------------------------------------- parser

void StreamParser::reset() {
  length_ = 0U;
  frame_size_ = 0U;
  crc_failures_ = 0U;
  discarded_bytes_ = 0U;
}

void StreamParser::shift(size_t count) {
  if (count >= length_) {
    length_ = 0U;
    return;
  }
  memmove(buffer_, buffer_ + count, length_ - count);
  length_ -= count;
}

bool StreamParser::feed(uint8_t byte) {
  // process() always leaves fewer than kMaxFrameSize bytes buffered.
  buffer_[length_++] = byte;
  return process();
}

bool StreamParser::process() {
  for (;;) {
    size_t skip = 0U;
    while (skip < length_ && buffer_[skip] != kMagic) ++skip;
    if (skip != 0U) {
      discarded_bytes_ += static_cast<uint32_t>(skip);
      shift(skip);
    }
    if (length_ < 2U) return false;

    const size_t expected = declaredLength(buffer_, length_);
    if (expected == 0U) {  // implausible len: this magic byte was noise
      ++discarded_bytes_;
      shift(1U);
      continue;
    }
    if (length_ < expected) return false;

    Frame decoded;
    if (decodeFrame(buffer_, expected, decoded) == Result::kOk) {
      memcpy(frame_, buffer_, expected);
      frame_size_ = expected;
      shift(expected);
      return true;
    }
    ++crc_failures_;  // bogus frame: resync from the next byte
    ++discarded_bytes_;
    shift(1U);
  }
}

}  // namespace BeeCoolerLink
