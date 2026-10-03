#include <unity.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <BeeCoolerLogic.h>
#include <BeeCoolerProtocol.h>
#include <BeeCoolerRecord.h>
#include <BeeCoolerVib.h>

using namespace BeeCoolerLogic;

void setUp() {}
void tearDown() {}

void test_epoch_roundtrip() {
  uint32_t e = 0;
  TEST_ASSERT_TRUE(civilToEpoch(2026, 10, 5, 12, 10, 0, e));
  TEST_ASSERT_EQUAL_UINT32(1791202200U, e);
  int y, mo, d, h, mi, s;
  epochToCivil(e, y, mo, d, h, mi, s);
  TEST_ASSERT_EQUAL(2026, y);
  TEST_ASSERT_EQUAL(10, mo);
  TEST_ASSERT_EQUAL(5, d);
  TEST_ASSERT_EQUAL(12, h);
  TEST_ASSERT_EQUAL(10, mi);
  TEST_ASSERT_FALSE(civilToEpoch(2026, 2, 30, 0, 0, 0, e));
  TEST_ASSERT_TRUE(civilToEpoch(2028, 2, 29, 0, 0, 0, e));
  TEST_ASSERT_TRUE(parseBuildEpoch("Oct  3 2026", "12:34:56", e));
  TEST_ASSERT_EQUAL_UINT32(1791030896U, e);
}

void test_scheduler() {
  TEST_ASSERT_EQUAL_UINT32(1200, nearestSlot(1199, 600));
  TEST_ASSERT_EQUAL_UINT32(1200, nearestSlot(1203, 600));
  TEST_ASSERT_EQUAL_UINT32(1200, nearestSlot(1200, 600));
  // finished at slot+12 s -> next slot is 588 s away, wake 2 s early
  TEST_ASSERT_EQUAL_UINT32(586, sleepSeconds(1212, 600, 2));
  TEST_ASSERT_TRUE(isLoraSlot(7200, 3600));
  TEST_ASSERT_FALSE(isLoraSlot(7800, 3600));
  TEST_ASSERT_EQUAL_UINT32(20, loraOffsetSeconds(1));
  TEST_ASSERT_EQUAL_UINT32(50, loraOffsetSeconds(2));
}

void test_power_policy_hysteresis() {
  PowerState s = {0, 0, 0};
  // two low readings are not enough
  TEST_ASSERT_FALSE(powerUpdate(s, 3400));
  TEST_ASSERT_FALSE(powerUpdate(s, 3400));
  TEST_ASSERT_EQUAL_UINT8(0, s.level);
  TEST_ASSERT_TRUE(powerUpdate(s, 3400));
  TEST_ASSERT_EQUAL_UINT8(1, s.level);
  // 3,55 V is above the entry (3,45) but below the exit (3,60): stays at 1
  for (int i = 0; i < 10; ++i) powerUpdate(s, 3550);
  TEST_ASSERT_EQUAL_UINT8(1, s.level);
  // an interrupted sequence restarts the count
  powerUpdate(s, 3650);
  powerUpdate(s, 3650);
  powerUpdate(s, 3550);
  powerUpdate(s, 3650);
  TEST_ASSERT_EQUAL_UINT8(1, s.level);
  for (int i = 0; i < 3; ++i) powerUpdate(s, 3650);
  TEST_ASSERT_EQUAL_UINT8(0, s.level);
  // a deep drop jumps straight to hibernation after 3 readings
  for (int i = 0; i < 3; ++i) powerUpdate(s, 3200);
  TEST_ASSERT_EQUAL_UINT8(3, s.level);
  TEST_ASSERT_EQUAL_UINT16(3400, median3(3400, 5000, 3300));
}

void test_level_params_fast() {
  LevelParams p0 = levelParams(0, 1);
  TEST_ASSERT_EQUAL_UINT32(600, p0.cycle_period_s);
  TEST_ASSERT_EQUAL_UINT32(3600, p0.lora_period_s);
  LevelParams f0 = levelParams(0, 20);
  TEST_ASSERT_EQUAL_UINT32(30, f0.cycle_period_s);
  TEST_ASSERT_EQUAL_UINT32(180, f0.lora_period_s);
  TEST_ASSERT_FALSE(levelParams(3, 1).radio);
  TEST_ASSERT_FALSE(levelParams(1, 1).measure_vibration);
}

void test_sht_decode() {
  // 25.00 C / 50 %RH: raw_t = (25+45)/175*65535 = 26214, raw_rh = 32768
  uint8_t b[6] = {0x66, 0x66, 0, 0x80, 0x00, 0};
  b[2] = shtCrc8(b, 2);
  b[5] = shtCrc8(b + 3, 2);
  int16_t t;
  uint16_t rh;
  TEST_ASSERT_TRUE(shtDecode(b, t, rh));
  TEST_ASSERT_INT16_WITHIN(5, 2500, t);
  TEST_ASSERT_UINT16_WITHIN(5, 5000, rh);
  b[1] ^= 1;
  TEST_ASSERT_FALSE(shtDecode(b, t, rh));
  // datasheet CRC example: 0xBEEF -> 0x92
  const uint8_t ex[2] = {0xBE, 0xEF};
  TEST_ASSERT_EQUAL_HEX8(0x92, shtCrc8(ex, 2));
}

void test_median_valid() {
  int16_t v[3] = {2500, 2510, 9000};
  bool ok[3] = {true, true, true};
  int16_t out;
  TEST_ASSERT_TRUE(medianValid(v, ok, 3, out));
  TEST_ASSERT_EQUAL_INT16(2510, out);
  ok[2] = false;
  TEST_ASSERT_TRUE(medianValid(v, ok, 3, out));
  TEST_ASSERT_EQUAL_INT16(2505, out);
  ok[0] = ok[1] = false;
  TEST_ASSERT_FALSE(medianValid(v, ok, 3, out));
}

void test_stuck_detection() {
  StuckHistory h;
  memset(&h, 0, sizeof(h));
  for (size_t i = 0; i < StuckHistory::kLength; ++i) {
    stuckPush(h, 3000, 6000, static_cast<int16_t>(2800 + i * 20));
  }
  TEST_ASSERT_TRUE(stuckDetected(h));
  stuckPush(h, 3100, 6000, 3500);  // inside moves: not stuck
  TEST_ASSERT_FALSE(stuckDetected(h));
}

void test_base64() {
  char out[32];
  TEST_ASSERT_EQUAL(8, base64Encode((const uint8_t*)"foobar"
                                    , 6, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("Zm9vYmFy", out);
  base64Encode((const uint8_t*)"fo", 2, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Zm8=", out);
  base64Encode((const uint8_t*)"f", 1, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Zg==", out);
  TEST_ASSERT_EQUAL(0, base64Encode((const uint8_t*)"foobar", 6, out, 5));
}

void test_crc32_chain_matches_protocol() {
  uint8_t data[100];
  for (int i = 0; i < 100; ++i) data[i] = (uint8_t)(i * 7 + 3);
  uint32_t s = BeeCoolerRecord::crc32Begin();
  s = BeeCoolerRecord::crc32Update(s, data, 32);
  s = BeeCoolerRecord::crc32Update(s, data + 32, 68);
  TEST_ASSERT_EQUAL_HEX32(BeeCoolerProtocol::generateCrc32(data, 100),
                          BeeCoolerRecord::crc32End(s));
}

// 1600 Hz, 10 s, a sine of known amplitude on the three axes.
static void fillSine(int16_t* s, size_t n, float hz, float amp_mg,
                     float dc_lsb) {
  for (size_t i = 0; i < n; ++i) {
    const float v = amp_mg / BeeCoolerVib::kMgPerLsb *
                    sinf(2.0f * 3.14159265f * hz * i / 1600.0f);
    for (int a = 0; a < 3; ++a) {
      s[3 * i + a] = (int16_t)lroundf(v + (a == 2 ? dc_lsb : 0.0f));
    }
  }
}

void test_vib_band_and_rms() {
  const size_t n = 16000;
  int16_t* s = (int16_t*)malloc(n * 3 * sizeof(int16_t));
  // 1 g on z as DC (gravity) plus a 150 Hz tone of 100 mg amplitude per axis
  fillSine(s, n, 150.0f, 100.0f, 256.0f);
  BeeCoolerVib::Features f = BeeCoolerVib::compute(s, n, 1600.0f);
  TEST_ASSERT_TRUE(f.valid);
  TEST_ASSERT_EQUAL(61, f.segments);
  // per-axis RMS = 100/sqrt(2) = 70.7 mg; vector over 3 axes = 122.5 mg
  TEST_ASSERT_FLOAT_WITHIN(6.0f, 122.5f, f.rms_mg);
  // peak of the vector module: 100*sqrt(3) = 173 mg
  TEST_ASSERT_FLOAT_WITHIN(8.0f, 173.2f, f.peak_mg);
  // the 100-200 Hz band holds the energy, the others are far below
  TEST_ASSERT_GREATER_THAN_FLOAT(f.band_db[0] + 20.0f, f.band_db[1]);
  TEST_ASSERT_GREATER_THAN_FLOAT(f.band_db[2] + 20.0f, f.band_db[1]);
  TEST_ASSERT_GREATER_THAN_FLOAT(f.band_db[3] + 20.0f, f.band_db[1]);
  // band energy ~ total power: 3 * 100^2/2 = 15000 mg^2 -> 41.8 dB
  TEST_ASSERT_FLOAT_WITHIN(1.5f, 41.8f, f.band_db[1]);

  // DC alone and a 20 Hz tone (below the high-pass) are strongly attenuated
  fillSine(s, n, 20.0f, 100.0f, 256.0f);
  BeeCoolerVib::Features low = BeeCoolerVib::compute(s, n, 1600.0f);
  TEST_ASSERT_LESS_THAN_FLOAT(f.rms_mg / 4.0f, low.rms_mg);

  // a 450 Hz tone lands in the 350-600 Hz band
  fillSine(s, n, 450.0f, 100.0f, 0.0f);
  BeeCoolerVib::Features hi = BeeCoolerVib::compute(s, n, 1600.0f);
  TEST_ASSERT_GREATER_THAN_FLOAT(hi.band_db[2] + 20.0f, hi.band_db[3]);
  free(s);
}

void test_vib_record_conversion() {
  TEST_ASSERT_EQUAL_UINT16(1225, BeeCoolerVib::toRecordMg10(122.5f));
  TEST_ASSERT_EQUAL_UINT16(65534, BeeCoolerVib::toRecordMg10(1e9f));
  TEST_ASSERT_EQUAL_UINT16(0, BeeCoolerVib::toRecordMg10(NAN));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_epoch_roundtrip);
  RUN_TEST(test_scheduler);
  RUN_TEST(test_power_policy_hysteresis);
  RUN_TEST(test_level_params_fast);
  RUN_TEST(test_sht_decode);
  RUN_TEST(test_median_valid);
  RUN_TEST(test_stuck_detection);
  RUN_TEST(test_base64);
  RUN_TEST(test_crc32_chain_matches_protocol);
  RUN_TEST(test_vib_band_and_rms);
  RUN_TEST(test_vib_record_conversion);
  return UNITY_END();
}
