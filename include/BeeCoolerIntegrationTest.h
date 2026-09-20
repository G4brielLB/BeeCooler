#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Temporary, integration-test-only payload schema. It is deliberately kept
// outside BeeCoolerProtocol and must not be treated as the field telemetry
// format. All multibyte values use network byte order (big-endian).
namespace BeeCoolerIntegrationTest {

constexpr uint8_t kSchemaVersion = 2U;
constexpr uint16_t kNominalSampleIntervalMs = 500U;
constexpr size_t kTargetRecordCount = 60U;
constexpr size_t kBatchHeaderSize = 9U;
constexpr size_t kRecordWireSize = 20U;
constexpr size_t kAckPayloadSize = 6U;
constexpr size_t kNormalBatchWireSize =
    kBatchHeaderSize + kTargetRecordCount * kRecordWireSize;

constexpr int16_t kUnavailableSigned = INT16_MIN;
constexpr uint16_t kUnavailableUnsigned = UINT16_MAX;

struct Sample {
  uint16_t sample_index;
  uint16_t elapsed_ms;
  float internal_temperature_c;
  float internal_humidity_percent;
  float external_temperature_c;
  float external_humidity_percent;
  float acceleration_x_g;
  float acceleration_y_g;
  float acceleration_z_g;
  float acceleration_magnitude_g;
};

struct BatchHeader {
  uint8_t schema_version;
  uint16_t record_count;
  uint16_t nominal_sample_interval_ms;
  uint32_t batch_start_ms;
};

struct AckPayload {
  uint32_t acked_sequence_id;
  uint16_t acked_fragment_index;
};

inline void writeUint16(uint8_t* output, uint16_t value) {
  output[0] = static_cast<uint8_t>(value >> 8U);
  output[1] = static_cast<uint8_t>(value);
}

inline void writeUint32(uint8_t* output, uint32_t value) {
  output[0] = static_cast<uint8_t>(value >> 24U);
  output[1] = static_cast<uint8_t>(value >> 16U);
  output[2] = static_cast<uint8_t>(value >> 8U);
  output[3] = static_cast<uint8_t>(value);
}

inline uint16_t readUint16(const uint8_t* input) {
  return static_cast<uint16_t>((static_cast<uint16_t>(input[0]) << 8U) |
                               static_cast<uint16_t>(input[1]));
}

inline uint32_t readUint32(const uint8_t* input) {
  return (static_cast<uint32_t>(input[0]) << 24U) |
         (static_cast<uint32_t>(input[1]) << 16U) |
         (static_cast<uint32_t>(input[2]) << 8U) |
         static_cast<uint32_t>(input[3]);
}

inline int16_t scaleSigned(float value, float scale) {
  if (!isfinite(value)) {
    return kUnavailableSigned;
  }
  const float scaled = value * scale;
  if (scaled <= -32767.0f) {
    return -32767;
  }
  if (scaled >= 32767.0f) {
    return 32767;
  }
  return static_cast<int16_t>(lroundf(scaled));
}

inline uint16_t scaleUnsigned(float value, float scale) {
  if (!isfinite(value) || value < 0.0f) {
    return kUnavailableUnsigned;
  }
  const float scaled = value * scale;
  if (scaled >= 65534.0f) {
    return 65534U;
  }
  return static_cast<uint16_t>(lroundf(scaled));
}

inline float decodeSigned(int16_t value, float scale) {
  return value == kUnavailableSigned ? NAN : value / scale;
}

inline float decodeUnsigned(uint16_t value, float scale) {
  return value == kUnavailableUnsigned ? NAN : value / scale;
}

inline bool serializeRecord(const Sample& sample, uint8_t* output,
                            size_t output_capacity) {
  if (output == nullptr || output_capacity < kRecordWireSize) {
    return false;
  }
  writeUint16(output + 0U, sample.sample_index);
  writeUint16(output + 2U, sample.elapsed_ms);
  writeUint16(output + 4U, static_cast<uint16_t>(
                                scaleSigned(sample.internal_temperature_c, 100.0f)));
  writeUint16(output + 6U,
              scaleUnsigned(sample.internal_humidity_percent, 100.0f));
  writeUint16(output + 8U, static_cast<uint16_t>(
                                scaleSigned(sample.external_temperature_c, 100.0f)));
  writeUint16(output + 10U,
              scaleUnsigned(sample.external_humidity_percent, 100.0f));
  writeUint16(output + 12U, static_cast<uint16_t>(
                                 scaleSigned(sample.acceleration_x_g, 10000.0f)));
  writeUint16(output + 14U, static_cast<uint16_t>(
                                 scaleSigned(sample.acceleration_y_g, 10000.0f)));
  writeUint16(output + 16U, static_cast<uint16_t>(
                                 scaleSigned(sample.acceleration_z_g, 10000.0f)));
  writeUint16(output + 18U,
              scaleUnsigned(sample.acceleration_magnitude_g, 10000.0f));
  return true;
}

inline bool deserializeRecord(const uint8_t* input, size_t input_size,
                              Sample& sample) {
  if (input == nullptr || input_size < kRecordWireSize) {
    return false;
  }
  sample.sample_index = readUint16(input + 0U);
  sample.elapsed_ms = readUint16(input + 2U);
  sample.internal_temperature_c =
      decodeSigned(static_cast<int16_t>(readUint16(input + 4U)), 100.0f);
  sample.internal_humidity_percent = decodeUnsigned(readUint16(input + 6U), 100.0f);
  sample.external_temperature_c =
      decodeSigned(static_cast<int16_t>(readUint16(input + 8U)), 100.0f);
  sample.external_humidity_percent = decodeUnsigned(readUint16(input + 10U), 100.0f);
  sample.acceleration_x_g =
      decodeSigned(static_cast<int16_t>(readUint16(input + 12U)), 10000.0f);
  sample.acceleration_y_g =
      decodeSigned(static_cast<int16_t>(readUint16(input + 14U)), 10000.0f);
  sample.acceleration_z_g =
      decodeSigned(static_cast<int16_t>(readUint16(input + 16U)), 10000.0f);
  sample.acceleration_magnitude_g =
      decodeUnsigned(readUint16(input + 18U), 10000.0f);
  return true;
}

inline bool serializeBatch(const Sample* samples, size_t record_count,
                           uint16_t nominal_sample_interval_ms,
                           uint32_t batch_start_ms,
                           uint8_t* output, size_t output_capacity,
                           size_t& output_size) {
  output_size = 0U;
  if ((samples == nullptr && record_count != 0U) || output == nullptr ||
      record_count > UINT16_MAX ||
      record_count > (SIZE_MAX - kBatchHeaderSize) / kRecordWireSize) {
    return false;
  }
  const size_t required_size =
      kBatchHeaderSize + record_count * kRecordWireSize;
  if (output_capacity < required_size) {
    return false;
  }
  output[0] = kSchemaVersion;
  writeUint16(output + 1U, static_cast<uint16_t>(record_count));
  writeUint16(output + 3U, nominal_sample_interval_ms);
  writeUint32(output + 5U, batch_start_ms);
  for (size_t i = 0U; i < record_count; ++i) {
    if (!serializeRecord(samples[i], output + kBatchHeaderSize +
                                         i * kRecordWireSize,
                         output_capacity - kBatchHeaderSize -
                             i * kRecordWireSize)) {
      return false;
    }
  }
  output_size = required_size;
  return true;
}

inline bool deserializeBatchHeader(const uint8_t* input, size_t input_size,
                                   BatchHeader& header) {
  if (input == nullptr || input_size < kBatchHeaderSize) {
    return false;
  }
  header.schema_version = input[0];
  header.record_count = readUint16(input + 1U);
  header.nominal_sample_interval_ms = readUint16(input + 3U);
  header.batch_start_ms = readUint32(input + 5U);
  if (header.schema_version != kSchemaVersion) {
    return false;
  }
  return input_size ==
         kBatchHeaderSize +
             static_cast<size_t>(header.record_count) * kRecordWireSize;
}

inline void serializeAck(const AckPayload& ack, uint8_t output[kAckPayloadSize]) {
  writeUint32(output, ack.acked_sequence_id);
  writeUint16(output + 4U, ack.acked_fragment_index);
}

inline bool deserializeAck(const uint8_t* input, size_t input_size,
                           AckPayload& ack) {
  if (input == nullptr || input_size != kAckPayloadSize) {
    return false;
  }
  ack.acked_sequence_id = readUint32(input);
  ack.acked_fragment_index = readUint16(input + 4U);
  return true;
}

}  // namespace BeeCoolerIntegrationTest
