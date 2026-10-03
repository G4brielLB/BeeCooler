#ifndef BEE_COOLER_LINK_H
#define BEE_COOLER_LINK_H

// LoRa link protocol v2 (Arquitetura v1.2, section 6). Little-endian.
// Incompatible with the ESP-NOW protocol v1 in BeeCoolerProtocol, whose CRC32
// implementation is reused here.

#include <stddef.h>
#include <stdint.h>

#include <BeeCoolerRecord.h>

namespace BeeCoolerLink {

constexpr uint8_t kMagic = 0xBEU;
constexpr uint8_t kVersion = 2U;
constexpr size_t kHeaderSize = 13U;
constexpr size_t kCrcSize = 4U;
constexpr size_t kMaxRecordsPerFrame = 4U;
constexpr size_t kMaxPayload = kMaxRecordsPerFrame * BeeCoolerRecord::kRecordSize;
constexpr size_t kMaxFrameSize = kHeaderSize + kMaxPayload + kCrcSize;  // 113
constexpr size_t kAckSize = 21U;
constexpr size_t kStatusPayloadSize = 36U;
constexpr size_t kHelloPayloadSize = 10U;
constexpr size_t kEventPayloadSize = 25U;
constexpr size_t kMaxAttempts = 4U;

enum class FrameType : uint8_t {
  kData = 0x1U,
  kStatus = 0x2U,
  kHello = 0x3U,
  kEvent = 0x4U,
  kAck = 0x8U,
};

enum class Result : uint8_t {
  kOk = 0U,
  kBadArgument,
  kBufferTooSmall,
  kBadMagic,
  kBadLength,
  kBadVersion,
  kBadType,
  kBadCrc,
  kBadPayload,
};

// Header shared by DATA / STATUS / HELLO / EVENT frames.
struct Header {
  FrameType type;
  uint8_t node_id;
  uint8_t boot_id;     // boot_count mod 256
  uint16_t frame_seq;  // retransmissions reuse it
  uint8_t attempt;     // 1..4
  uint32_t first_seq;
  uint8_t n_rec;
};

struct Status {
  BeeCoolerRecord::RecordV1 record;  // latest record; its seq is header.first_seq
  uint16_t backlog;
  uint16_t sd_free_mb;
  uint8_t sd_errors;
  uint8_t reset_reason;
  uint8_t rssi;               // RSSI of the last ACK, encoded as -dBm
  uint8_t failed_sessions;
  uint16_t vbat_tx_mv;
  uint16_t fw_version;
};

struct Hello {
  uint16_t fw_version;
  uint8_t reset_reason;
  uint8_t rtc_valid;
  uint32_t last_seq;
  uint16_t boot_count;
};

struct Ack {
  uint8_t node_id;
  uint16_t frame_seq;
  uint32_t acked_up_to;  // highest contiguous seq persisted by the gateway
  uint32_t epoch;        // gateway UTC; 0 when its NTP is not valid
  uint8_t rssi;          // -dBm of the received frame
  int8_t snr;
  uint8_t pending_command;
};

// Decoded view of any valid frame. Payload points into the caller's buffer.
struct Frame {
  bool is_ack;
  Header header;
  Ack ack;
  const uint8_t* payload;
  size_t payload_length;
};

// ---- Encoding. `out_size` receives the frame length.
Result encodeFrame(const Header& header, const uint8_t* payload,
                   size_t payload_length, uint8_t* out, size_t capacity,
                   size_t& out_size);
Result encodeData(Header header, const BeeCoolerRecord::RecordV1* records,
                  uint8_t count, uint8_t* out, size_t capacity,
                  size_t& out_size);
Result encodeStatus(Header header, const Status& status, uint8_t* out,
                    size_t capacity, size_t& out_size);
Result encodeHello(Header header, const Hello& hello, uint8_t* out,
                   size_t capacity, size_t& out_size);
Result encodeAck(const Ack& ack, uint8_t* out, size_t capacity,
                 size_t& out_size);

// Rewrites the attempt number of an encoded non-ACK frame and refreshes its
// CRC (retransmissions reuse everything else).
Result setAttempt(uint8_t* frame, size_t size, uint8_t attempt);

// ---- Decoding. Validates magic, len, version, type and CRC32.
Result decodeFrame(const uint8_t* data, size_t size, Frame& frame);
Result decodeStatus(const uint8_t* payload, size_t length, Status& status);
Result decodeHello(const uint8_t* payload, size_t length, Hello& hello);

// Frame length declared by the first two bytes, or 0 when not plausible.
size_t declaredLength(const uint8_t* data, size_t available);

// Extracts frames from a raw UART byte stream: synchronizes on the magic byte,
// uses `len`, validates the CRC and resynchronizes after garbage or a frame
// split by the module (P-HDR). Never allocates.
class StreamParser {
 public:
  StreamParser() { reset(); }
  void reset();
  // Feeds one byte. Returns true when `frame()` holds a new valid frame.
  // After a true result call poll() until it returns false: a resync may have
  // left another complete frame in the internal buffer.
  bool feed(uint8_t byte);
  bool poll() { return process(); }
  const uint8_t* frame() const { return frame_; }
  size_t frameSize() const { return frame_size_; }
  uint32_t crcFailures() const { return crc_failures_; }
  uint32_t discardedBytes() const { return discarded_bytes_; }

 private:
  bool process();
  void shift(size_t count);
  uint8_t buffer_[kMaxFrameSize];
  size_t length_;
  uint8_t frame_[kMaxFrameSize];
  size_t frame_size_;
  uint32_t crc_failures_;
  uint32_t discarded_bytes_;
};

}  // namespace BeeCoolerLink

#endif  // BEE_COOLER_LINK_H
