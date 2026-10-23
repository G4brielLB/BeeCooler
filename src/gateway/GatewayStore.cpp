#include "GatewayStore.h"

#include <LittleFS.h>

namespace GatewayStore {

namespace {

constexpr const char* kQueuePath = "/queue.bin";
constexpr const char* kHeadPath = "/qhead.bin";
constexpr const char* kQuarantinePath = "/quar.bin";

SemaphoreHandle_t gMutex = nullptr;
NodeRecordState gNodes[kMaxNodes];
bool gNodeLoaded[kMaxNodes] = {};

struct Lock {
  Lock() { xSemaphoreTake(gMutex, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(gMutex); }
};

uint32_t readHeadLocked() {
  File f = LittleFS.open(kHeadPath, "r");
  if (!f || f.size() != sizeof(uint32_t)) return 0;
  uint32_t head = 0;
  f.read(reinterpret_cast<uint8_t*>(&head), sizeof(head));
  return head;
}

bool writeHeadLocked(uint32_t head) {
  File f = LittleFS.open(kHeadPath, "w");
  if (!f) return false;
  const bool ok = f.write(reinterpret_cast<const uint8_t*>(&head), sizeof(head)) ==
                  sizeof(head);
  f.close();
  return ok;
}

uint32_t totalLocked() {
  File f = LittleFS.open(kQueuePath, "r");
  return f ? static_cast<uint32_t>(f.size() / kQueueItemSize) : 0;
}

// Drops the consumed prefix: delete everything when drained, otherwise rewrite
// the remaining items once enough of them are dead weight.
void compactLocked(uint32_t head, uint32_t total) {
  if (head >= total) {
    LittleFS.remove(kQueuePath);
    LittleFS.remove(kHeadPath);
    return;
  }
  if (head < kQueueCompactThreshold) return;
  File src = LittleFS.open(kQueuePath, "r");
  File dst = LittleFS.open("/queue.tmp", "w");
  if (!src || !dst) return;
  src.seek(static_cast<size_t>(head) * kQueueItemSize);
  uint8_t buffer[kQueueItemSize];
  bool ok = true;
  for (uint32_t i = head; i < total && ok; ++i) {
    ok = src.read(buffer, sizeof(buffer)) == sizeof(buffer) &&
         dst.write(buffer, sizeof(buffer)) == sizeof(buffer);
  }
  src.close();
  dst.close();
  if (!ok) {
    LittleFS.remove("/queue.tmp");
    return;
  }
  // Crash safety: a stale head over the new, shorter file would skip valid
  // items, whereas head = 0 over the old file only resends (the cloud ingests
  // idempotently). So the head is zeroed first, then rename() atomically
  // replaces the queue (LittleFS rename over an existing file is atomic).
  writeHeadLocked(0);
  LittleFS.rename("/queue.tmp", kQueuePath);
}

}  // namespace

SemaphoreHandle_t mutex() { return gMutex; }

bool begin() {
  gMutex = xSemaphoreCreateMutex();
  if (!LittleFS.begin(true)) return false;  // true: format on first use
  Lock lock;
  // A power cut mid-write can leave a partial item: cut the tail.
  File f = LittleFS.open(kQueuePath, "r");
  if (f) {
    const size_t size = f.size();
    f.close();
    const size_t whole = (size / kQueueItemSize) * kQueueItemSize;
    if (whole != size) {
      File in = LittleFS.open(kQueuePath, "r");
      File out = LittleFS.open("/queue.tmp", "w");
      uint8_t buffer[kQueueItemSize];
      for (size_t off = 0; off < whole && in && out; off += kQueueItemSize) {
        if (in.read(buffer, sizeof(buffer)) != sizeof(buffer)) break;
        out.write(buffer, sizeof(buffer));
      }
      in.close();
      out.close();
      LittleFS.rename("/queue.tmp", kQueuePath);
    }
  }
  const uint32_t total = totalLocked();
  if (readHeadLocked() > total) writeHeadLocked(0);
  return true;
}

bool appendItem(const QueueItem& item) {
  Lock lock;
  File f = LittleFS.open(kQueuePath, "a");
  if (!f) return false;
  const bool ok = f.write(reinterpret_cast<const uint8_t*>(&item), sizeof(item)) ==
                  sizeof(item);
  f.close();  // close commits to flash: only then may the ACK be sent
  return ok;
}

uint32_t pendingCount() {
  Lock lock;
  const uint32_t total = totalLocked();
  const uint32_t head = readHeadLocked();
  return total > head ? total - head : 0;
}

uint32_t quarantineCount() {
  Lock lock;
  File f = LittleFS.open(kQuarantinePath, "r");
  return f ? static_cast<uint32_t>(f.size() / kQueueItemSize) : 0;
}

uint32_t peek(QueueItem* out, uint32_t max) {
  Lock lock;
  const uint32_t total = totalLocked();
  const uint32_t head = readHeadLocked();
  if (total <= head) return 0;
  File f = LittleFS.open(kQueuePath, "r");
  if (!f) return 0;
  f.seek(static_cast<size_t>(head) * kQueueItemSize);
  uint32_t n = 0;
  while (n < max && head + n < total &&
         f.read(reinterpret_cast<uint8_t*>(&out[n]), sizeof(QueueItem)) ==
             sizeof(QueueItem)) {
    ++n;
  }
  return n;
}

bool consume(uint32_t count) {
  Lock lock;
  const uint32_t total = totalLocked();
  const uint32_t head = min(total, readHeadLocked() + count);
  if (!writeHeadLocked(head)) return false;
  compactLocked(head, total);
  return true;
}

bool quarantine(uint32_t count) {
  QueueItem item;
  {
    Lock lock;
    const uint32_t total = totalLocked();
    const uint32_t head = readHeadLocked();
    File src = LittleFS.open(kQueuePath, "r");
    File dst = LittleFS.open(kQuarantinePath, "a");
    if (!src || !dst) return false;
    src.seek(static_cast<size_t>(head) * kQueueItemSize);
    for (uint32_t i = 0; i < count && head + i < total; ++i) {
      if (src.read(reinterpret_cast<uint8_t*>(&item), sizeof(item)) != sizeof(item)) break;
      dst.write(reinterpret_cast<const uint8_t*>(&item), sizeof(item));
    }
  }
  return consume(count);
}

NodeRecordState* node(uint8_t node_id) {
  if (node_id == 0) return nullptr;
  Lock lock;
  const uint8_t slot = static_cast<uint8_t>(node_id % kMaxNodes);
  // Open addressing is overkill for a handful of nodes: one slot per id modulo
  // kMaxNodes, and a clash is reported as "no room".
  NodeRecordState& s = gNodes[slot];
  if (!gNodeLoaded[slot]) {
    char path[16];
    snprintf(path, sizeof(path), "/n%u.bin", node_id);
    memset(&s, 0, sizeof(s));
    File f = LittleFS.open(path, "r");
    if (f && f.size() == sizeof(s)) f.read(reinterpret_cast<uint8_t*>(&s), sizeof(s));
    if (s.magic != kNodeStateMagic) {
      memset(&s, 0, sizeof(s));
      s.magic = kNodeStateMagic;
      s.node_id = node_id;
    }
    gNodeLoaded[slot] = true;
  }
  return s.node_id == node_id ? &s : nullptr;
}

bool saveNode(const NodeRecordState& state) {
  Lock lock;
  char path[16];
  snprintf(path, sizeof(path), "/n%u.bin", state.node_id);
  File f = LittleFS.open(path, "w");
  if (!f) return false;
  const bool ok = f.write(reinterpret_cast<const uint8_t*>(&state), sizeof(state)) ==
                  sizeof(state);
  f.close();
  return ok;
}

size_t freeBytes() {
  Lock lock;
  return LittleFS.totalBytes() - LittleFS.usedBytes();
}

}  // namespace GatewayStore
