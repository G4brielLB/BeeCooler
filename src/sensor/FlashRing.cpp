#include "FlashRing.h"

using namespace BeeCoolerRecord;

bool FlashRing::begin(const char* label, uint8_t subtype) {
  partition_ = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, static_cast<esp_partition_subtype_t>(subtype),
      label);
  if (partition_ == nullptr) return false;
  slots_ = partition_->size / kSlotSize;
  return slots_ >= kSlotsPerSector;
}

bool FlashRing::readSlot(uint32_t index, RingSlot& slot) {
  return esp_partition_read(partition_, static_cast<size_t>(index) * kSlotSize,
                            &slot, sizeof(slot)) == ESP_OK;
}

bool FlashRing::slotBlank(uint32_t index) {
  uint8_t raw[kSlotSize];
  if (esp_partition_read(partition_, static_cast<size_t>(index) * kSlotSize,
                         raw, sizeof(raw)) != ESP_OK) {
    return false;
  }
  for (size_t i = 0; i < sizeof(raw); ++i) {
    if (raw[i] != 0xFF) return false;
  }
  return true;
}

bool FlashRing::eraseSectorOf(uint32_t index) {
  const size_t sector = (index / kSlotsPerSector) * kSectorSize;
  return esp_partition_erase_range(partition_, sector, kSectorSize) == ESP_OK;
}

bool FlashRing::append(uint32_t seq, const RecordV1& record) {
  if (partition_ == nullptr || seq == 0) return false;
  const uint32_t index = seq % slots_;
  const bool first_of_sector = (index % kSlotsPerSector) == 0;

  // Erase when entering a sector. After an anomaly (first write in a sector
  // that is not at its start, e.g. a freshly flashed partition) the previous
  // slot is also checked so valid neighbours are never erased.
  bool need_erase = first_of_sector;
  if (!need_erase && !slotBlank(index)) {
    RingSlot previous;
    need_erase = !readSlot(index - 1, previous) || !validateSlot(previous) ||
                 previous.seq != seq - 1;
    if (!need_erase) return false;  // neighbour is valid but this slot is dirty
  }
  if (need_erase && !eraseSectorOf(index)) return false;

  RingSlot slot;
  buildSlot(seq, record, slot);
  return esp_partition_write(partition_, static_cast<size_t>(index) * kSlotSize,
                             &slot, sizeof(slot)) == ESP_OK;
}

bool FlashRing::read(uint32_t seq, RecordV1& record) {
  if (partition_ == nullptr || seq == 0) return false;
  RingSlot slot;
  if (!readSlot(seq % slots_, slot) || !validateSlot(slot) || slot.seq != seq) {
    return false;
  }
  record = slot.record;
  return true;
}

uint32_t FlashRing::oldestSeq(uint32_t last_seq) const {
  if (last_seq < slots_) return 1;
  return last_seq - slots_ + 1;
}

uint32_t FlashRing::recoverLastSeq(uint32_t hint) {
  if (partition_ == nullptr) return 0;

  // Fast path: the hinted slot is valid; also pick up a slot written just
  // before the state was saved (power cut between the two).
  if (hint != 0) {
    RecordV1 scratch;
    if (read(hint, scratch)) {
      uint32_t last = hint;
      while (read(last + 1, scratch)) ++last;
      return last;
    }
  }

  // Slow path: the first slot of every sector, then the best sector in full.
  uint32_t best_sector = UINT32_MAX;
  uint32_t best_seq = 0;
  const uint32_t sectors = slots_ / kSlotsPerSector;
  RingSlot slot;
  for (uint32_t s = 0; s < sectors; ++s) {
    if (readSlot(s * kSlotsPerSector, slot) && validateSlot(slot) &&
        slot.seq > best_seq) {
      best_seq = slot.seq;
      best_sector = s;
    }
  }
  if (best_sector == UINT32_MAX) return 0;

  uint32_t last = best_seq;
  for (uint32_t i = 1; i < kSlotsPerSector; ++i) {
    if (readSlot(best_sector * kSlotsPerSector + i, slot) &&
        validateSlot(slot) && slot.seq > last) {
      last = slot.seq;
    }
  }
  return last;
}
