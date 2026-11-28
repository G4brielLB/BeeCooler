#include "BeeCoolerML.h"

#include <math.h>
#include <string.h>

#include "beecooler_tinyml.h"

#ifndef BEE_TINYML_INT8
#define BEE_TINYML_INT8 0  // 1 = pesos INT8 (diferenca < 0,01 C na v0)
#endif

namespace BeeCoolerML {

namespace {

// C++03: assercao estatica sem static_assert.
typedef char StateFits[(sizeof(bcml_state_t) <= sizeof(State)) ? 1 : -1];

const uint32_t kSegmentGapS = 3600U;  // = config.QC["segment_gap_s"]
const int16_t kInvI16 = -32768;       // sentinelas do RecordV1
const uint16_t kInvU16 = 0xFFFFU;

bcml_state_t* raw(State& s) { return reinterpret_cast<bcml_state_t*>(s.words); }

float localHour(uint32_t ts_utc, int utc_offset_h) {
  const long day = 86400L;
  long t = static_cast<long>(ts_utc % 86400U) + static_cast<long>(utc_offset_h) * 3600L;
  t %= day;
  if (t < 0) t += day;
  return static_cast<float>(t) / 3600.0f;
}

bool inDay(float hour) { return hour >= BCML_DAY_H0 && hour < BCML_DAY_H1; }

}  // namespace

void reset(State& state) {
  memset(&state, 0, sizeof(state));
  bcml_reset(raw(state));
}

bool insideValid(int16_t t_in_c, uint16_t rh_in_c) {
  return t_in_c != kInvI16 && t_in_c >= 500 && t_in_c <= 6000 && rh_in_c != kInvU16 &&
         rh_in_c >= 50U && rh_in_c <= 10000U;
}

bool outsideValid(int16_t t_out_c, uint16_t rh_out_c) {
  return t_out_c != kInvI16 && t_out_c >= 500 && t_out_c <= 6000 && rh_out_c != kInvU16 &&
         rh_out_c > 50U && rh_out_c <= 10000U;
}

void step(State& state, uint32_t ts_utc, bool rtc_valid, int16_t t_in_c, uint16_t rh_in_c,
          int16_t t_out_c, uint16_t rh_out_c, int utc_offset_h, Result& out) {
  stepFloat(state, ts_utc, rtc_valid, t_in_c / 100.0f, insideValid(t_in_c, rh_in_c), rh_in_c / 100.0f,
            t_out_c / 100.0f, rh_out_c / 100.0f, outsideValid(t_out_c, rh_out_c), utc_offset_h, out);
}

void stepFloat(State& state, uint32_t ts_utc, bool rtc_valid, float t_in, bool in_ok, float rh_in,
               float t_out, float rh_out, bool out_ok, int utc_offset_h, Result& out) {
  bcml_state_t* s = raw(state);
  out.pump = false;
  out.model_ok = false;
  out.t_in_used = in_ok ? t_in : NAN;
  out.t_pred = out.t_in_used;
  out.t_virtual = NAN;
  out.hour_local = NAN;

  if (!rtc_valid) {  // sem hora local nao ha janela diurna nem EMA confiavel
    s->pump_on = 0;
    out.reason = kNoClock;
    return;
  }
  // Lacuna longa (ou relogio que voltou): recomeca como um segmento novo.
  if (s->ts != 0 && (ts_utc <= s->ts || ts_utc - s->ts > kSegmentGapS)) bcml_reset(s);

  const float hour = localHour(ts_utc, utc_offset_h);
  out.hour_local = hour;
  bcml_feat_t f;

  if (out_ok) {
    // Com o interno invalido, o historico de T_in recebe primeiro o ultimo valor
    // conhecido e depois a estimativa do sensor virtual (redundancia).
    const float t_seed = in_ok ? t_in : (s->n > 0 ? s->tin[s->n - 1] : t_out);
    const float rh_seed = in_ok ? rh_in : 0.0f;
    bcml_update(s, ts_utc, t_seed, rh_seed, t_out, rh_out, hour, &f);
    out.t_virtual = bcml_virtual_tin(&f, BEE_TINYML_INT8);
    if (!in_ok) {
      s->tin[s->n - 1] = out.t_virtual;
      f.t_in = out.t_virtual;
      out.t_in_used = out.t_virtual;
    }
    out.model_ok = in_ok && f.age_min >= BCML_WARMUP_MIN;  // a previsao foi treinada com T_in medida
    if (inDay(hour) && out.model_ok) out.t_pred = bcml_forecast(&f, BEE_TINYML_INT8);
    else out.t_pred = out.t_in_used;
    out.reason = !in_ok ? kInVirtual : !inDay(hour) ? kNight : out.model_ok ? kOk : kWarmup;
  } else if (in_ok) {
    out.reason = inDay(hour) ? kOutInvalid : kNight;
  } else {
    s->pump_on = 0;
    out.reason = kNoSensor;
    return;
  }

  out.pump = bcml_decide(s, ts_utc, hour, out.t_in_used, out.t_pred, out.model_ok) != 0;
}

int horizonMinutes() { return BCML_HORIZON_MIN; }

const char* reasonName(uint8_t reason) {
  switch (reason) {
    case kOk: return "ok";
    case kWarmup: return "aquecendo";
    case kNight: return "noite";
    case kOutInvalid: return "ext_invalido";
    case kInVirtual: return "int_virtual";
    case kNoSensor: return "sem_sensor";
    case kNoClock: return "sem_relogio";
    default: return "?";
  }
}

}  // namespace BeeCoolerML
