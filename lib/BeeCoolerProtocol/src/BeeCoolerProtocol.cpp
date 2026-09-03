#include "BeeCoolerProtocol.h"

#include <limits.h>
#include <string.h>

namespace BeeCoolerProtocol {
namespace {

constexpr size_t kVersionOffset = 0U;
constexpr size_t kMessageTypeOffset = 1U;
constexpr size_t kNodeIdOffset = 2U;
constexpr size_t kBatchIdOffset = 6U;
constexpr size_t kSequenceIdOffset = 10U;
constexpr size_t kFragmentIndexOffset = 14U;
constexpr size_t kFragmentCountOffset = 16U;
constexpr size_t kPayloadLengthOffset = 18U;

void writeUint16(uint8_t* output, uint16_t value) {
  output[0] = static_cast<uint8_t>(value >> 8U);
  output[1] = static_cast<uint8_t>(value);
}

void writeUint32(uint8_t* output, uint32_t value) {
  output[0] = static_cast<uint8_t>(value >> 24U);
  output[1] = static_cast<uint8_t>(value >> 16U);
  output[2] = static_cast<uint8_t>(value >> 8U);
  output[3] = static_cast<uint8_t>(value);
}

uint16_t readUint16(const uint8_t* input) {
  return static_cast<uint16_t>((static_cast<uint16_t>(input[0]) << 8U) |
                               static_cast<uint16_t>(input[1]));
}

uint32_t readUint32(const uint8_t* input) {
  return (static_cast<uint32_t>(input[0]) << 24U) |
         (static_cast<uint32_t>(input[1]) << 16U) |
         (static_cast<uint32_t>(input[2]) << 8U) |
         static_cast<uint32_t>(input[3]);
}

bool isSupportedMessageType(MessageType type) {
  switch (type) {
    case MessageType::kTelemetryBatchFragment:
    case MessageType::kRawAccelerometerBatchFragment:
    case MessageType::kAck:
    case MessageType::kTimeSynchronization:
    case MessageType::kBatchCommitted:
      return true;
  }
  return false;
}

Status validatePacketMetadata(const Packet& packet) {
  if (packet.version != kProtocolVersion) {
    return Status::kUnsupportedVersion;
  }
  if (!isSupportedMessageType(packet.message_type)) {
    return Status::kUnsupportedMessageType;
  }
  if (packet.payload_length > kMaxPayloadSize) {
    return Status::kInvalidPayloadLength;
  }
  if (packet.fragment_count == 0U ||
      packet.fragment_index >= packet.fragment_count) {
    return Status::kInvalidFragmentMetadata;
  }
  return Status::kOk;
}

}  // namespace

size_t serializedSize(const Packet& packet) {
  if (packet.payload_length > kMaxPayloadSize) {
    return 0U;
  }
  return kSerializedHeaderSize + packet.payload_length + kCrc32Size;
}

uint32_t generateCrc32(const uint8_t* data, size_t length) {
  if (data == nullptr && length != 0U) {
    return 0U;
  }

  uint32_t crc = UINT32_C(0xFFFFFFFF);
  for (size_t i = 0U; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0U; bit < 8U; ++bit) {
      const uint32_t mask = 0U - (crc & 1U);
      crc = (crc >> 1U) ^ (UINT32_C(0xEDB88320) & mask);
    }
  }
  return crc ^ UINT32_C(0xFFFFFFFF);
}

Status validateSerializedCrc(const uint8_t* serialized, size_t serialized_size) {
  if (serialized == nullptr) {
    return Status::kNullArgument;
  }
  if (serialized_size > kMaxSerializedPacketSize) {
    return Status::kPacketTooLarge;
  }
  if (serialized_size < kMinSerializedPacketSize) {
    return Status::kTruncatedPacket;
  }

  const size_t crc_offset = serialized_size - kCrc32Size;
  const uint32_t expected_crc = readUint32(serialized + crc_offset);
  return generateCrc32(serialized, crc_offset) == expected_crc
             ? Status::kOk
             : Status::kCrcMismatch;
}

Status serialize(const Packet& packet, uint8_t* output,
                 size_t output_capacity, size_t& output_size) {
  output_size = 0U;
  if (output == nullptr) {
    return Status::kNullArgument;
  }
  const Status metadata_status = validatePacketMetadata(packet);
  if (metadata_status != Status::kOk) {
    return metadata_status;
  }

  const size_t required_size = serializedSize(packet);
  if (required_size > kMaxSerializedPacketSize) {
    return Status::kPacketTooLarge;
  }
  if (output_capacity < required_size) {
    return Status::kBufferTooSmall;
  }

  output[kVersionOffset] = packet.version;
  output[kMessageTypeOffset] = static_cast<uint8_t>(packet.message_type);
  writeUint32(output + kNodeIdOffset, packet.node_id);
  writeUint32(output + kBatchIdOffset, packet.batch_id);
  writeUint32(output + kSequenceIdOffset, packet.sequence_id);
  writeUint16(output + kFragmentIndexOffset, packet.fragment_index);
  writeUint16(output + kFragmentCountOffset, packet.fragment_count);
  writeUint16(output + kPayloadLengthOffset, packet.payload_length);
  if (packet.payload_length != 0U) {
    memcpy(output + kSerializedHeaderSize, packet.payload,
           packet.payload_length);
  }

  const size_t crc_offset = kSerializedHeaderSize + packet.payload_length;
  writeUint32(output + crc_offset, generateCrc32(output, crc_offset));
  output_size = required_size;
  return Status::kOk;
}

Status deserialize(const uint8_t* serialized, size_t serialized_size,
                   Packet& packet) {
  if (serialized == nullptr) {
    return Status::kNullArgument;
  }
  if (serialized_size > kMaxSerializedPacketSize) {
    return Status::kPacketTooLarge;
  }
  if (serialized_size < kMinSerializedPacketSize) {
    return Status::kTruncatedPacket;
  }
  if (serialized[kVersionOffset] != kProtocolVersion) {
    return Status::kUnsupportedVersion;
  }

  const uint16_t payload_length =
      readUint16(serialized + kPayloadLengthOffset);
  if (payload_length > kMaxPayloadSize) {
    return Status::kInvalidPayloadLength;
  }
  const size_t expected_size =
      kSerializedHeaderSize + payload_length + kCrc32Size;
  if (serialized_size < expected_size) {
    return Status::kTruncatedPacket;
  }
  if (serialized_size != expected_size) {
    return Status::kInvalidPayloadLength;
  }

  const Status crc_status = validateSerializedCrc(serialized, serialized_size);
  if (crc_status != Status::kOk) {
    return crc_status;
  }

  Packet decoded = {};
  decoded.version = serialized[kVersionOffset];
  decoded.message_type =
      static_cast<MessageType>(serialized[kMessageTypeOffset]);
  decoded.node_id = readUint32(serialized + kNodeIdOffset);
  decoded.batch_id = readUint32(serialized + kBatchIdOffset);
  decoded.sequence_id = readUint32(serialized + kSequenceIdOffset);
  decoded.fragment_index = readUint16(serialized + kFragmentIndexOffset);
  decoded.fragment_count = readUint16(serialized + kFragmentCountOffset);
  decoded.payload_length = payload_length;

  const Status metadata_status = validatePacketMetadata(decoded);
  if (metadata_status != Status::kOk) {
    return metadata_status;
  }
  if (payload_length != 0U) {
    memcpy(decoded.payload, serialized + kSerializedHeaderSize, payload_length);
  }
  packet = decoded;
  return Status::kOk;
}

Status fragmentBatch(const uint8_t* batch, size_t batch_size,
                     const FragmentationOptions& options, Packet* fragments,
                     size_t fragment_capacity, size_t& fragment_count) {
  fragment_count = 0U;
  if ((batch == nullptr && batch_size != 0U) || fragments == nullptr) {
    return Status::kNullArgument;
  }
  if (!isSupportedMessageType(options.message_type)) {
    return Status::kUnsupportedMessageType;
  }

  const size_t required_fragments =
      batch_size == 0U ? 1U : 1U + ((batch_size - 1U) / kMaxPayloadSize);
  if (required_fragments > UINT16_MAX) {
    return Status::kTooManyFragments;
  }
  if (required_fragments > fragment_capacity) {
    return Status::kFragmentCapacityTooSmall;
  }
  if (options.first_sequence_id >
      UINT32_MAX - static_cast<uint32_t>(required_fragments - 1U)) {
    return Status::kSequenceOverflow;
  }

  size_t offset = 0U;
  for (size_t i = 0U; i < required_fragments; ++i) {
    Packet& fragment = fragments[i];
    fragment = {};
    fragment.version = kProtocolVersion;
    fragment.message_type = options.message_type;
    fragment.node_id = options.node_id;
    fragment.batch_id = options.batch_id;
    fragment.sequence_id =
        options.first_sequence_id + static_cast<uint32_t>(i);
    fragment.fragment_index = static_cast<uint16_t>(i);
    fragment.fragment_count = static_cast<uint16_t>(required_fragments);
    const size_t remaining = batch_size - offset;
    const size_t payload_size =
        remaining < kMaxPayloadSize ? remaining : kMaxPayloadSize;
    fragment.payload_length = static_cast<uint16_t>(payload_size);
    if (payload_size != 0U) {
      memcpy(fragment.payload, batch + offset, payload_size);
      offset += payload_size;
    }
  }

  fragment_count = required_fragments;
  return Status::kOk;
}

Status reassembleBatch(const Packet* fragments, size_t fragment_count,
                       uint8_t* output, size_t output_capacity,
                       size_t& output_size) {
  output_size = 0U;
  if (fragments == nullptr || output == nullptr) {
    return Status::kNullArgument;
  }
  if (fragment_count == 0U || fragment_count > UINT16_MAX) {
    return Status::kInvalidFragmentMetadata;
  }

  const Packet& first = fragments[0];
  const Status first_status = validatePacketMetadata(first);
  if (first_status != Status::kOk) {
    return first_status;
  }
  if (first.fragment_count != fragment_count) {
    return Status::kMissingFragment;
  }

  size_t required_size = 0U;
  for (size_t i = 0U; i < fragment_count; ++i) {
    const Packet& fragment = fragments[i];
    const Status metadata_status = validatePacketMetadata(fragment);
    if (metadata_status != Status::kOk) {
      return metadata_status;
    }
    if (fragment.node_id != first.node_id ||
        fragment.batch_id != first.batch_id ||
        fragment.message_type != first.message_type ||
        fragment.fragment_count != first.fragment_count) {
      return Status::kFragmentSetMismatch;
    }
    if (required_size > output_capacity ||
        fragment.payload_length > output_capacity - required_size) {
      return Status::kBufferTooSmall;
    }
    required_size += fragment.payload_length;

    for (size_t other = i + 1U; other < fragment_count; ++other) {
      if (fragment.fragment_index == fragments[other].fragment_index) {
        return Status::kDuplicateFragment;
      }
    }
  }

  if (required_size > output_capacity) {
    return Status::kBufferTooSmall;
  }
  size_t offset = 0U;
  for (size_t index = 0U; index < fragment_count; ++index) {
    const Packet* matching = nullptr;
    for (size_t i = 0U; i < fragment_count; ++i) {
      if (fragments[i].fragment_index == index) {
        matching = &fragments[i];
        break;
      }
    }
    if (matching == nullptr) {
      return Status::kMissingFragment;
    }
    if (matching->payload_length != 0U) {
      memcpy(output + offset, matching->payload, matching->payload_length);
      offset += matching->payload_length;
    }
  }
  output_size = offset;
  return Status::kOk;
}

}  // namespace BeeCoolerProtocol
