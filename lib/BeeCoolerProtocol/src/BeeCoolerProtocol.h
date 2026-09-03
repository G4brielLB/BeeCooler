#ifndef BEE_COOLER_PROTOCOL_H
#define BEE_COOLER_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

namespace BeeCoolerProtocol {

// Protocol v1 uses network byte order (big-endian) for every multibyte field.
constexpr uint8_t kProtocolVersion = 1U;
constexpr size_t kSerializedHeaderSize = 20U;
constexpr size_t kCrc32Size = 4U;
constexpr size_t kMaxSerializedPacketSize = 200U;
constexpr size_t kMaxPayloadSize =
    kMaxSerializedPacketSize - kSerializedHeaderSize - kCrc32Size;
constexpr size_t kMinSerializedPacketSize =
    kSerializedHeaderSize + kCrc32Size;

static_assert(kMaxPayloadSize == 176U, "Unexpected protocol payload capacity");

enum class MessageType : uint8_t {
  kTelemetryBatchFragment = 1U,
  kRawAccelerometerBatchFragment = 2U,
  kAck = 3U,
  kTimeSynchronization = 4U,
  kBatchCommitted = 5U,
};

enum class Status : uint8_t {
  kOk = 0U,
  kNullArgument,
  kBufferTooSmall,
  kPacketTooLarge,
  kUnsupportedVersion,
  kUnsupportedMessageType,
  kInvalidPayloadLength,
  kInvalidFragmentMetadata,
  kTruncatedPacket,
  kCrcMismatch,
  kTooManyFragments,
  kSequenceOverflow,
  kFragmentCapacityTooSmall,
  kFragmentSetMismatch,
  kDuplicateFragment,
  kMissingFragment,
};

struct Packet {
  uint8_t version;
  MessageType message_type;
  uint32_t node_id;
  uint32_t batch_id;
  uint32_t sequence_id;
  uint16_t fragment_index;
  uint16_t fragment_count;
  uint16_t payload_length;
  uint8_t payload[kMaxPayloadSize];
};

struct FragmentationOptions {
  MessageType message_type;
  uint32_t node_id;
  uint32_t batch_id;
  uint32_t first_sequence_id;
};

size_t serializedSize(const Packet& packet);

// CRC-32/ISO-HDLC (polynomial 0x04C11DB7, reflected representation
// 0xEDB88320, initial/final XOR 0xFFFFFFFF).
uint32_t generateCrc32(const uint8_t* data, size_t length);

// Validates the trailing wire-format CRC against every preceding byte.
Status validateSerializedCrc(const uint8_t* serialized, size_t serialized_size);

Status serialize(const Packet& packet, uint8_t* output,
                 size_t output_capacity, size_t& output_size);

Status deserialize(const uint8_t* serialized, size_t serialized_size,
                   Packet& packet);

// Returns one zero-payload fragment for an empty logical batch. No heap
// allocation is performed; the caller supplies the bounded packet array.
Status fragmentBatch(const uint8_t* batch, size_t batch_size,
                     const FragmentationOptions& options, Packet* fragments,
                     size_t fragment_capacity, size_t& fragment_count);

// Fragments may be supplied in any order. All must belong to the same logical
// batch and each fragment index must occur exactly once.
Status reassembleBatch(const Packet* fragments, size_t fragment_count,
                       uint8_t* output, size_t output_capacity,
                       size_t& output_size);

}  // namespace BeeCoolerProtocol

#endif  // BEE_COOLER_PROTOCOL_H
