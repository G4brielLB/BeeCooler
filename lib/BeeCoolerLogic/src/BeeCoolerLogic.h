#ifndef BEE_COOLER_LOGIC_H
#define BEE_COOLER_LOGIC_H

// Hardware-independent node logic, testable on the host: calendar math, slot
// scheduling (4.1, 6.5), battery power policy (8.3), SHT30 helpers (4.3) and
// base64 for the cloud payload (Integracao em Nuvem 5.2).

#include <stddef.h>
#include <stdint.h>

namespace BeeCoolerLogic {

// ---------------------------------------------------------------- calendar
// Seconds since 1970-01-01 UTC. Returns false for impossible dates.
bool civilToEpoch(int year, int month, int day, int hour, int minute,
                  int second, uint32_t& epoch);
void epochToCivil(uint32_t epoch, int& year, int& month, int& day, int& hour,
                  int& minute, int& second);
// Parses the compiler's __DATE__ ("Oct  3 2026") and __TIME__ ("12:34:56").
bool parseBuildEpoch(const char* date, const char* time, uint32_t& epoch);

// -------------------------------------------------------------- scheduling
// Nearest multiple of `period_s` to `now` (a wake a few seconds before a slot
// still belongs to that slot).
uint32_t nearestSlot(uint32_t now, uint32_t period_s);
// Seconds to deep-sleep so that the next wake lands `margin_s` before the next
// slot after `now`.
uint32_t sleepSeconds(uint32_t now, uint32_t period_s, uint32_t margin_s);
// True when this slot is also a LoRa session slot.
bool isLoraSlot(uint32_t slot, uint32_t lora_period_s);
// Offset after the slot at which node `node_id` transmits (6.5):
// 20 s + (node_id - 1) * 30 s.
uint32_t loraOffsetSeconds(uint8_t node_id);

// ------------------------------------------------------------ power policy
constexpr uint8_t kPowerLevels = 4U;

struct PowerThresholds {
  uint16_t enter_mv[3];  // levels 1..3: enter below
  uint16_t exit_mv[3];   // levels 1..3: leave above
};
constexpr PowerThresholds kDefaultThresholds = {{3450U, 3350U, 3250U},
                                                {3600U, 3500U, 3450U}};

// Behaviour per level; periods are production values (seconds).
struct LevelParams {
  uint32_t cycle_period_s;
  bool measure_sht;
  bool measure_vibration;
  bool radio;
  uint32_t lora_period_s;
  bool status_only;  // sessions carry a STATUS but no DATA backlog
};
// `time_divisor` > 1 compresses every period (BEE_TEST_FAST uses 20).
LevelParams levelParams(uint8_t level, uint32_t time_divisor);

struct PowerState {
  uint8_t level;
  uint8_t pending_level;
  uint8_t streak;
};
constexpr uint8_t kLevelChangeReadings = 3U;

// Feeds one resting battery reading (already median-filtered by the caller).
// Returns true when the level changed.
bool powerUpdate(PowerState& state, uint16_t vbat_mv,
                 const PowerThresholds& thresholds = kDefaultThresholds);

uint16_t median3(uint16_t a, uint16_t b, uint16_t c);

// ------------------------------------------------------------------- SHT30
uint8_t shtCrc8(const uint8_t* data, size_t length);  // poly 0x31, init 0xFF
// Converts a 6-byte SHT30 answer. False when a CRC fails or a value is outside
// the physical range (-40..125 C, 0..100 %RH). Outputs 0.01 C / 0.01 %RH.
bool shtDecode(const uint8_t* bytes, int16_t& t_centi, uint16_t& rh_centi);
// Median of the valid entries of three readings; false when none are valid.
// Two valid readings give their average.
bool medianValid(const int16_t* values, const bool* valid, size_t count,
                 int16_t& out);

// Stuck-internal-sensor detection over the last 6 h (36 records): internal T
// and RH practically constant while the external T varies.
struct StuckHistory {
  static constexpr size_t kLength = 36U;
  int16_t t_in[kLength];
  uint16_t rh_in[kLength];
  int16_t t_out[kLength];
  uint8_t count;
  uint8_t head;
};
void stuckPush(StuckHistory& history, int16_t t_in, uint16_t rh_in,
               int16_t t_out);
bool stuckDetected(const StuckHistory& history);

// ------------------------------------------------------------------ base64
size_t base64EncodedSize(size_t length);
// Writes base64 plus a terminating NUL; returns the length without the NUL, or
// 0 when `capacity` is too small.
size_t base64Encode(const uint8_t* data, size_t length, char* out,
                    size_t capacity);

}  // namespace BeeCoolerLogic

#endif  // BEE_COOLER_LOGIC_H
