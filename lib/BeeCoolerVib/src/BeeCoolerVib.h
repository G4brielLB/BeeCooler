#ifndef BEE_COOLER_VIB_H
#define BEE_COOLER_VIB_H

// Vibration features computed on the node and recomputed on the PC from the
// RAW window (Arquitetura v1.2, 4.5, D47):
//   DC removal -> 2nd-order Butterworth high-pass (~50 Hz) -> RMS / peak
//   Welch PSD (Hann 512, 50 % overlap) summed over the 3 axes -> 4 bands in dB

#include <stddef.h>
#include <stdint.h>

namespace BeeCoolerVib {

constexpr size_t kSegment = 512U;
constexpr size_t kHop = kSegment / 2U;
constexpr size_t kBands = 4U;
constexpr float kHighPassHz = 50.0f;
// ADXL345 full resolution: 3.9 mg/LSB.
constexpr float kMgPerLsb = 3.9f;
constexpr float kBandEdgesHz[kBands + 1U] = {50.0f, 100.0f, 200.0f, 350.0f,
                                             600.0f};

struct Features {
  bool valid;
  float rms_mg;       // vector RMS after DC removal and high-pass
  float peak_mg;      // max |a - mean| (vector module), DC removed only
  float band_db[kBands];  // 10*log10 of band energy in (mg)^2
  size_t segments;
};

// `samples` is interleaved x,y,z int16 raw counts, `n` samples per axis.
// Needs at least kSegment samples per axis.
// Uses shared static scratch buffers; calls must not overlap across tasks.
Features compute(const int16_t* samples, size_t n, float sample_rate_hz);

// Record fields: 0.1 mg units clamped to 0..65534.
uint16_t toRecordMg10(float mg);

}  // namespace BeeCoolerVib

#endif  // BEE_COOLER_VIB_H
