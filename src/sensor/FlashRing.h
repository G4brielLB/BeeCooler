#pragma once

#include <Arduino.h>
#include <esp_partition.h>

#include <BeeCoolerRecord.h>

// Ring buffer of 32-byte slots in the `beelog` flash partition
// (Arquitetura v1.2, 5.6). slot = seq mod N; a 4 KB sector is erased right
// before it receives its first slot. Reading by seq needs no search.
class FlashRing {
 public:
  bool begin(const char* label, uint8_t subtype);

  uint32_t slotCount() const { return slots_; }

  // Appends the record with the given seq. Writes are 32 B, with the marker
  // last, so a power cut leaves a slot whose CRC fails (never a half-valid one).
  bool append(uint32_t seq, const BeeCoolerRecord::RecordV1& record);

  // True when the slot holds exactly this seq with a valid CRC.
  bool read(uint32_t seq, BeeCoolerRecord::RecordV1& record);

  // UC-10. `hint` is the last seq believed written (0 = unknown). Returns the
  // highest valid seq found, or 0 when the ring is empty.
  uint32_t recoverLastSeq(uint32_t hint);

  // Oldest seq still retained when the newest is `last_seq`.
  uint32_t oldestSeq(uint32_t last_seq) const;

 private:
  bool readSlot(uint32_t index, BeeCoolerRecord::RingSlot& slot);
  bool slotBlank(uint32_t index);
  bool eraseSectorOf(uint32_t index);

  const esp_partition_t* partition_ = nullptr;
  uint32_t slots_ = 0;
};
