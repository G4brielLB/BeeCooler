// Teste nativo do TinyML: pio test -e native -f test_tinyml
// Confere a lib BeeCoolerML (C) contra as saidas calculadas em Python sobre a
// coleta de campo N01 (vectors.h, gerado por analysis/ml/run_all.py) e os
// caminhos de sensor invalido, lacuna e relogio que o Python nao exercita.
#include <unity.h>

#include <math.h>

#include <BeeCoolerML.h>

#include "vectors.h"

using namespace BeeCoolerML;

static const int kUtcOffset = -3;

void setUp() {}
void tearDown() {}

static void feedValid(State& st, uint32_t ts0, int n, float t_in, float t_out, Result& r) {
  for (int i = 0; i < n; ++i) {
    stepFloat(st, ts0 + 600U * i, true, t_in, true, 60.0f, t_out, 40.0f, true, kUtcOffset, r);
  }
}

void test_vectors_match_python() {
  State st;
  reset(st);
  Result r;
  int n_fc = 0, bad_fc = 0, bad_vs = 0, bad_pump = 0;
  for (int i = 0; i < BCML_N_VEC; ++i) {
    const bcml_vec_t& v = BCML_VEC[i];
    stepFloat(st, v.ts, true, v.t_in, true, v.rh_in, v.t_out, v.rh_out, true, kUtcOffset, r);
    if (r.reason == kOk) {
      ++n_fc;
      if (fabsf(r.t_pred - v.exp_fc) > 2e-3f) ++bad_fc;
    }
    if (fabsf(r.t_virtual - v.exp_vs) > 2e-3f) ++bad_vs;
    if (static_cast<int>(r.pump) != v.exp_pump) ++bad_pump;
  }
  TEST_ASSERT_TRUE(n_fc > BCML_N_VEC / 3);  // a maior parte do dia com o modelo ativo
  TEST_ASSERT_EQUAL_INT(0, bad_fc);
  TEST_ASSERT_EQUAL_INT(0, bad_vs);
  TEST_ASSERT_EQUAL_INT(0, bad_pump);
}

void test_record_validity() {
  TEST_ASSERT_TRUE(insideValid(3500, 6000));
  TEST_ASSERT_FALSE(insideValid(-32768, 6000));
  TEST_ASSERT_FALSE(insideValid(3500, 0xFFFF));
  TEST_ASSERT_TRUE(outsideValid(3000, 4000));
  TEST_ASSERT_FALSE(outsideValid(2332, 0));   // falha de 27/11: RH_ext = 0 com T plausivel
  TEST_ASSERT_FALSE(outsideValid(9000, 4000));
}

// 2026-11-25 15:00 UTC = 12:00 local
static const uint32_t kNoonUtc = 1795618800U;

void test_warmup_then_model() {
  State st;
  reset(st);
  Result r;
  feedValid(st, kNoonUtc, 1, 36.0f, 40.0f, r);
  TEST_ASSERT_EQUAL_UINT8(kWarmup, r.reason);
  TEST_ASSERT_TRUE(r.pump);  // 36 C >= t_on decide pela T_in mesmo aquecendo
  feedValid(st, kNoonUtc + 600U, 7, 36.0f, 40.0f, r);
  TEST_ASSERT_EQUAL_UINT8(kOk, r.reason);
  TEST_ASSERT_TRUE(r.model_ok);
}

void test_inside_invalid_uses_virtual() {
  State st;
  reset(st);
  Result r;
  feedValid(st, kNoonUtc, 10, 37.0f, 40.0f, r);
  stepFloat(st, kNoonUtc + 6000U, true, NAN, false, NAN, 40.0f, 40.0f, true, kUtcOffset, r);
  TEST_ASSERT_EQUAL_UINT8(kInVirtual, r.reason);
  TEST_ASSERT_FALSE(r.model_ok);
  TEST_ASSERT_FALSE(isnan(r.t_virtual));
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, r.t_virtual, r.t_in_used);
}

void test_outside_invalid_falls_back_to_inside() {
  State st;
  reset(st);
  Result r;
  feedValid(st, kNoonUtc, 10, 30.0f, 40.0f, r);
  stepFloat(st, kNoonUtc + 6000U, true, 37.0f, true, 60.0f, 23.0f, 0.0f, false, kUtcOffset, r);
  TEST_ASSERT_EQUAL_UINT8(kOutInvalid, r.reason);
  TEST_ASSERT_FALSE(r.model_ok);
  TEST_ASSERT_TRUE(r.pump);  // 37 C medida
}

void test_no_sensor_and_no_clock_force_off() {
  State st;
  reset(st);
  Result r;
  feedValid(st, kNoonUtc, 10, 37.0f, 40.0f, r);
  TEST_ASSERT_TRUE(r.pump);
  stepFloat(st, kNoonUtc + 6000U, true, NAN, false, NAN, NAN, NAN, false, kUtcOffset, r);
  TEST_ASSERT_EQUAL_UINT8(kNoSensor, r.reason);
  TEST_ASSERT_FALSE(r.pump);
  stepFloat(st, kNoonUtc + 6600U, false, 37.0f, true, 60.0f, 40.0f, 40.0f, true, kUtcOffset, r);
  TEST_ASSERT_EQUAL_UINT8(kNoClock, r.reason);
  TEST_ASSERT_FALSE(r.pump);
}

void test_night_and_gap() {
  State st;
  reset(st);
  Result r;
  feedValid(st, kNoonUtc, 10, 37.0f, 40.0f, r);
  TEST_ASSERT_TRUE(r.pump);
  // 21:00 local (00:00 UTC do dia seguinte), lacuna de ~7 h: reinicia e fica desligada
  stepFloat(st, kNoonUtc + 9U * 3600U, true, 37.0f, true, 60.0f, 33.0f, 50.0f, true, kUtcOffset, r);
  TEST_ASSERT_EQUAL_UINT8(kNight, r.reason);
  TEST_ASSERT_FALSE(r.pump);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 21.0f, r.hour_local);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_vectors_match_python);
  RUN_TEST(test_record_validity);
  RUN_TEST(test_warmup_then_model);
  RUN_TEST(test_inside_invalid_uses_virtual);
  RUN_TEST(test_outside_invalid_falls_back_to_inside);
  RUN_TEST(test_no_sensor_and_no_clock_force_off);
  RUN_TEST(test_night_and_gap);
  return UNITY_END();
}
