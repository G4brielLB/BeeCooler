#ifndef BEE_COOLER_RECORD_H
#define BEE_COOLER_RECORD_H

// Canonical record and storage layouts (Arquitetura v1.2, section 5).
// Every multibyte field is little-endian (native on ESP32 and x86 hosts).

#include <stddef.h>
#include <stdint.h>

namespace BeeCoolerRecord {

constexpr int16_t kInvalidI16 = INT16_MIN;
constexpr uint16_t kInvalidU16 = UINT16_MAX;
constexpr uint8_t kInvalidU8 = 0xFFU;

struct __attribute__((packed)) RecordV1 {
  uint32_t ts;         // epoch UTC (DS1302)
  int16_t t_in;        // 0.01 C
  uint16_t rh_in;      // 0.01 %
  int16_t t_out;       // 0.01 C
  uint16_t rh_out;     // 0.01 %
  uint16_t vbat_mv;    // mV, measured at rest
  uint16_t vib_rms;    // 0.1 mg
  uint16_t vib_peak;   // 0.1 mg
  uint8_t band_db[4];  // 50-100 / 100-200 / 200-350 / 350-600 Hz
  uint16_t flags;      // RecordFlag bits
};
static_assert(sizeof(RecordV1) == 24U, "Canonical record v1 must be 24 bytes");

constexpr size_t kRecordSize = sizeof(RecordV1);

enum RecordFlag : uint16_t {
  kShtInFail = 1U << 0,
  kShtOutFail = 1U << 1,
  kAdxlFail = 1U << 2,
  kFifoOverrun = 1U << 3,
  kRtcInvalid = 1U << 4,
  kClockAdjusted = 1U << 5,
  kColdBoot = 1U << 6,
  kSdFail = 1U << 7,
  kVibSkipped = 1U << 8,
  // bits 9-10: power level
  kShtInStuck = 1U << 11,
  kMechEvent = 1U << 12,
  kCharging = 1U << 13,
};

constexpr uint8_t kPowerLevelShift = 9U;
constexpr uint16_t kPowerLevelMask = 0x3U << kPowerLevelShift;

uint16_t setPowerLevel(uint16_t flags, uint8_t level);
uint8_t getPowerLevel(uint16_t flags);

// Initializes every field with its "invalid" sentinel and flags = 0.
void initInvalid(RecordV1& record);

// band_db = round(2 * (dB + 100)), 0.5 dB steps from -100 dB. 0xFF = invalid.
uint8_t encodeBandDb(float db);
float decodeBandDb(uint8_t encoded);  // NAN when invalid

uint8_t crc8(const uint8_t* data, size_t length);   // poly 0x07, init 0x00
uint16_t crc16(const uint8_t* data, size_t length); // CCITT-FALSE, init 0xFFFF

// Incremental CRC-32/ISO-HDLC (same result as BeeCoolerProtocol::generateCrc32
// over the concatenated data). Start with crc32Begin(), feed with
// crc32Update(), finish with crc32End().
uint32_t crc32Begin();
uint32_t crc32Update(uint32_t state, const uint8_t* data, size_t length);
uint32_t crc32End(uint32_t state);

// ---- Flash ring slot (5.6): [seq u32][record 24 B][CRC16 2 B][marker 2 B]
constexpr uint16_t kSlotMarker = 0xA55AU;
constexpr size_t kSlotSize = 32U;
constexpr size_t kSectorSize = 4096U;
constexpr size_t kSlotsPerSector = kSectorSize / kSlotSize;  // 128

struct __attribute__((packed)) RingSlot {
  uint32_t seq;
  RecordV1 record;
  uint16_t crc;
  uint16_t marker;
};
static_assert(sizeof(RingSlot) == kSlotSize, "Ring slot must be 32 bytes");

void buildSlot(uint32_t seq, const RecordV1& record, RingSlot& slot);
// True when marker and CRC16 are valid.
bool validateSlot(const RingSlot& slot);

// ---- microSD raw window header (5.8), 32 B, followed by payload and CRC32
constexpr size_t kRawHeaderSize = 32U;
struct __attribute__((packed)) RawHeader {
  char magic[4];  // "BEEV"
  uint8_t version;
  uint8_t node_id;
  uint8_t odr_code;
  uint8_t range_code;
  uint32_t seq;
  uint32_t ts_start;
  uint16_t n_samples;
  uint16_t fifo_overruns;
  int16_t t_in;
  uint16_t flags;
  uint8_t reserved[8];
};
static_assert(sizeof(RawHeader) == kRawHeaderSize, "Raw header must be 32 bytes");

// ---- microSD extended record (5.9), 64 B
constexpr size_t kExtendedRecordSize = 64U;
struct __attribute__((packed)) ExtendedRecord {
  uint32_t seq;
  RecordV1 record;
  uint16_t sht_raw[12];  // 3 reads x (T,RH) x 2 sensors
  uint16_t wake_ms;
  uint16_t sd_write_ms;
  uint16_t processing_ms;
  uint8_t fifo_overruns;
  uint16_t vbat_tx_mv;
  uint8_t reserved;
  uint16_t crc;
};
static_assert(sizeof(ExtendedRecord) == kExtendedRecordSize,
              "Extended record must be 64 bytes");

void finalizeExtended(ExtendedRecord& extended);

}  // namespace BeeCoolerRecord

#endif  // BEE_COOLER_RECORD_H
