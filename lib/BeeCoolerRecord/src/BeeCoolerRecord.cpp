#include "BeeCoolerRecord.h"

#include <math.h>
#include <string.h>

namespace BeeCoolerRecord {

uint16_t setPowerLevel(uint16_t flags, uint8_t level) {
  return static_cast<uint16_t>((flags & ~kPowerLevelMask) |
                               ((level & 0x3U) << kPowerLevelShift));
}

uint8_t getPowerLevel(uint16_t flags) {
  return static_cast<uint8_t>((flags & kPowerLevelMask) >> kPowerLevelShift);
}

void initInvalid(RecordV1& record) {
  memset(&record, 0, sizeof(record));
  record.t_in = kInvalidI16;
  record.t_out = kInvalidI16;
  record.rh_in = kInvalidU16;
  record.rh_out = kInvalidU16;
  record.vbat_mv = kInvalidU16;
  record.vib_rms = kInvalidU16;
  record.vib_peak = kInvalidU16;
  memset(record.band_db, kInvalidU8, sizeof(record.band_db));
}

uint8_t encodeBandDb(float db) {
  if (isnan(db)) return kInvalidU8;
  float scaled = roundf(2.0f * (db + 100.0f));
  if (scaled < 0.0f) scaled = 0.0f;
  if (scaled > 254.0f) scaled = 254.0f;  // 0xFF is reserved for "invalid"
  return static_cast<uint8_t>(scaled);
}

float decodeBandDb(uint8_t encoded) {
  if (encoded == kInvalidU8) return NAN;
  return static_cast<float>(encoded) / 2.0f - 100.0f;
}

uint8_t crc8(const uint8_t* data, size_t length) {
  uint8_t crc = 0x00U;
  for (size_t i = 0U; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0U; bit < 8U; ++bit) {
      crc = (crc & 0x80U) ? static_cast<uint8_t>((crc << 1) ^ 0x07U)
                          : static_cast<uint8_t>(crc << 1);
    }
  }
  return crc;
}

uint16_t crc16(const uint8_t* data, size_t length) {
  uint16_t crc = 0xFFFFU;
  for (size_t i = 0U; i < length; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (uint8_t bit = 0U; bit < 8U; ++bit) {
      crc = (crc & 0x8000U) ? static_cast<uint16_t>((crc << 1) ^ 0x1021U)
                            : static_cast<uint16_t>(crc << 1);
    }
  }
  return crc;
}

uint32_t crc32Begin() { return 0xFFFFFFFFU; }

uint32_t crc32Update(uint32_t state, const uint8_t* data, size_t length) {
  for (size_t i = 0U; i < length; ++i) {
    state ^= data[i];
    for (uint8_t bit = 0U; bit < 8U; ++bit) {
      state = (state & 1U) ? (state >> 1) ^ 0xEDB88320U : (state >> 1);
    }
  }
  return state;
}

uint32_t crc32End(uint32_t state) { return ~state; }

void buildSlot(uint32_t seq, const RecordV1& record, RingSlot& slot) {
  slot.seq = seq;
  slot.record = record;
  slot.marker = kSlotMarker;
  slot.crc = crc16(reinterpret_cast<const uint8_t*>(&slot),
                   offsetof(RingSlot, crc));
}

bool validateSlot(const RingSlot& slot) {
  if (slot.marker != kSlotMarker) return false;
  return slot.crc == crc16(reinterpret_cast<const uint8_t*>(&slot),
                           offsetof(RingSlot, crc));
}

void finalizeExtended(ExtendedRecord& extended) {
  extended.crc = crc16(reinterpret_cast<const uint8_t*>(&extended),
                       offsetof(ExtendedRecord, crc));
}

}  // namespace BeeCoolerRecord
