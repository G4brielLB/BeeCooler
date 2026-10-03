#include "RawSink.h"

#include <FS.h>
#include <SD.h>

#include <BeeCoolerLogic.h>

#include "SharedSpi.h"
#include "config.h"

using namespace BeeCoolerRecord;

bool SdRawSink::begin(int cs, uint8_t node_id, uint32_t epoch) {
  mounted_ = false;
  if (!SD.begin(cs, sharedSpi(), kSdSpiHz)) return false;
  if (SD.cardType() == CARD_NONE) {
    SD.end();
    return false;
  }

  int y, mo, d, h, mi, s;
  BeeCoolerLogic::epochToCivil(epoch, y, mo, d, h, mi, s);
  char node_dir[24];
  snprintf(node_dir, sizeof(node_dir), "/BEE/N%02u", node_id);
  // Without a valid clock (epoch 0) data goes to a fixed folder.
  if (epoch == 0) {
    snprintf(dir_, sizeof(dir_), "%s/NOCLOCK", node_dir);
  } else {
    snprintf(dir_, sizeof(dir_), "%s/%04d%02d%02d", node_dir, y, mo, d);
  }

  // mkdir is not recursive on the VFS layer.
  if (!SD.exists("/BEE") && !SD.mkdir("/BEE")) return false;
  if (!SD.exists(node_dir) && !SD.mkdir(node_dir)) return false;
  if (!SD.exists(dir_) && !SD.mkdir(dir_)) return false;
  mounted_ = true;
  return true;
}

// open(append) -> write -> flush -> close: nothing stays open during sleep.
bool SdRawSink::appendTo(const char* name, const uint8_t* a, size_t na,
                         const uint8_t* b, size_t nb, const uint8_t* c,
                         size_t nc) {
  if (!mounted_) return false;
  char path[48];
  snprintf(path, sizeof(path), "%s/%s", dir_, name);
  File f = SD.open(path, FILE_APPEND);
  if (!f) return false;
  bool ok = true;
  constexpr size_t kChunk = 4096;
  const uint8_t* parts[3] = {a, b, c};
  const size_t sizes[3] = {na, nb, nc};
  for (int p = 0; p < 3 && ok; ++p) {
    for (size_t off = 0; off < sizes[p] && ok; off += kChunk) {
      const size_t n = min(kChunk, sizes[p] - off);
      ok = f.write(parts[p] + off, n) == n;
    }
  }
  f.flush();
  f.close();
  return ok;
}

bool SdRawSink::writeWindow(const RawHeader& header, const int16_t* samples,
                            size_t n) {
  const size_t payload = n * 3U * sizeof(int16_t);
  uint32_t crc = crc32Begin();
  crc = crc32Update(crc, reinterpret_cast<const uint8_t*>(&header), sizeof(header));
  crc = crc32Update(crc, reinterpret_cast<const uint8_t*>(samples), payload);
  const uint32_t trailer = crc32End(crc);
  return appendTo("raw.bin", reinterpret_cast<const uint8_t*>(&header),
                  sizeof(header), reinterpret_cast<const uint8_t*>(samples),
                  payload, reinterpret_cast<const uint8_t*>(&trailer),
                  sizeof(trailer));
}

bool SdRawSink::writeExtended(const ExtendedRecord& record) {
  return appendTo("rec.bin", reinterpret_cast<const uint8_t*>(&record),
                  sizeof(record), nullptr, 0, nullptr, 0);
}

bool SdRawSink::logEvent(const char* line) {
  return appendTo("events.log", reinterpret_cast<const uint8_t*>(line),
                  strlen(line), reinterpret_cast<const uint8_t*>("\n"), 1,
                  nullptr, 0);
}

uint16_t SdRawSink::freeMegabytes() {
  if (!mounted_) return 0xFFFF;
  // usedBytes() walks the FAT: slow on a 16 GB card, so callers cache it.
  const uint64_t total = SD.totalBytes();
  const uint64_t used = SD.usedBytes();
  const uint64_t free_mb = (total > used ? total - used : 0) / (1024ULL * 1024ULL);
  return free_mb > 65534ULL ? 65534U : static_cast<uint16_t>(free_mb);
}

void SdRawSink::end() {
  if (mounted_) SD.end();
  mounted_ = false;
}
