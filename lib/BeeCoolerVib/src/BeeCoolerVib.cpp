#include "BeeCoolerVib.h"

#include <math.h>
#include <string.h>

namespace BeeCoolerVib {

namespace {

constexpr float kPi = 3.14159265358979323846f;

struct Biquad {
  float b0, b1, b2, a1, a2;
  float z1, z2;
  float step(float x) {  // transposed direct form II
    const float y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return y;
  }
};

Biquad makeHighPass(float fc, float fs) {
  const float k = tanf(kPi * fc / fs);
  const float q = 0.70710678f;  // Butterworth
  const float norm = 1.0f / (1.0f + k / q + k * k);
  Biquad b;
  b.b0 = norm;
  b.b1 = -2.0f * norm;
  b.b2 = norm;
  b.a1 = 2.0f * (k * k - 1.0f) * norm;
  b.a2 = (1.0f - k / q + k * k) * norm;
  b.z1 = b.z2 = 0.0f;
  return b;
}

// In-place iterative radix-2 FFT of kSegment points.
void fft(float* re, float* im, const float* cos_t, const float* sin_t) {
  const size_t n = kSegment;
  for (size_t i = 1U, j = 0U; i < n; ++i) {
    size_t bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) {
      const float tr = re[i];
      re[i] = re[j];
      re[j] = tr;
      const float ti = im[i];
      im[i] = im[j];
      im[j] = ti;
    }
  }
  for (size_t len = 2U; len <= n; len <<= 1) {
    const size_t half = len / 2U;
    const size_t step = n / len;
    for (size_t i = 0U; i < n; i += len) {
      for (size_t k = 0U; k < half; ++k) {
        const float wr = cos_t[k * step];
        const float wi = -sin_t[k * step];
        const size_t a = i + k;
        const size_t b = a + half;
        const float xr = re[b] * wr - im[b] * wi;
        const float xi = re[b] * wi + im[b] * wr;
        re[b] = re[a] - xr;
        im[b] = im[a] - xi;
        re[a] += xr;
        im[a] += xi;
      }
    }
  }
}

}  // namespace

uint16_t toRecordMg10(float mg) {
  if (!(mg >= 0.0f)) return 0U;
  const float v = mg * 10.0f + 0.5f;
  return v > 65534.0f ? 65534U : static_cast<uint16_t>(v);
}

Features compute(const int16_t* samples, size_t n, float fs) {
  Features out;
  memset(&out, 0, sizeof(out));
  if (samples == nullptr || n < kSegment || fs <= 0.0f) return out;

  // Reuse bounded scratch buffers outside the Arduino loop task's stack.
  // Together these need 11 KiB, exceeding its default 8 KiB stack.
  // Acquisition and processing call compute sequentially from one task.
  static float cos_t[kSegment / 2U];
  static float sin_t[kSegment / 2U];
  for (size_t k = 0U; k < kSegment / 2U; ++k) {
    const float a = 2.0f * kPi * static_cast<float>(k) / kSegment;
    cos_t[k] = cosf(a);
    sin_t[k] = sinf(a);
  }
  static float window[kSegment];
  float window_power = 0.0f;
  for (size_t i = 0U; i < kSegment; ++i) {
    window[i] = 0.5f - 0.5f * cosf(2.0f * kPi * i / kSegment);  // periodic Hann
    window_power += window[i] * window[i];
  }

  // Pass 1: per-axis mean.
  double sum[3] = {0.0, 0.0, 0.0};
  for (size_t i = 0U; i < n; ++i) {
    for (size_t a = 0U; a < 3U; ++a) sum[a] += samples[3U * i + a];
  }
  float mean[3];
  for (size_t a = 0U; a < 3U; ++a) mean[a] = static_cast<float>(sum[a] / n);

  // Peak: vector module of the DC-removed signal.
  float peak_sq = 0.0f;
  for (size_t i = 0U; i < n; ++i) {
    float m = 0.0f;
    for (size_t a = 0U; a < 3U; ++a) {
      const float d = (samples[3U * i + a] - mean[a]) * kMgPerLsb;
      m += d * d;
    }
    if (m > peak_sq) peak_sq = m;
  }
  out.peak_mg = sqrtf(peak_sq);

  // Pass 2: per axis, high-pass, RMS, and Welch accumulation.
  static float psd[kSegment / 2U + 1U];
  memset(psd, 0, sizeof(psd));
  float rms_sq_total = 0.0f;
  size_t segments = 0U;

  for (size_t a = 0U; a < 3U; ++a) {
    Biquad hp = makeHighPass(kHighPassHz, fs);
    static float segment[kSegment];
    size_t filled = 0U;
    double energy = 0.0;
    size_t axis_segments = 0U;
    for (size_t i = 0U; i < n; ++i) {
      const float x = (samples[3U * i + a] - mean[a]) * kMgPerLsb;
      const float y = hp.step(x);
      energy += static_cast<double>(y) * y;
      segment[filled++] = y;
      if (filled == kSegment) {
        static float re[kSegment];
        static float im[kSegment];
        for (size_t k = 0U; k < kSegment; ++k) {
          re[k] = segment[k] * window[k];
          im[k] = 0.0f;
        }
        fft(re, im, cos_t, sin_t);
        for (size_t k = 0U; k <= kSegment / 2U; ++k) {
          psd[k] += re[k] * re[k] + im[k] * im[k];
        }
        ++axis_segments;
        memmove(segment, segment + kHop, (kSegment - kHop) * sizeof(float));
        filled = kSegment - kHop;
      }
    }
    rms_sq_total += static_cast<float>(energy / n);
    segments = axis_segments;
  }
  out.rms_mg = sqrtf(rms_sq_total);
  out.segments = segments;

  // Welch scaling: one-sided PSD = 2*|X|^2 / (fs * sum(w^2)) averaged over the
  // segments of every axis; the axes are summed (not averaged).
  const float df = fs / kSegment;
  const float scale = 2.0f / (fs * window_power * static_cast<float>(segments));
  for (size_t b = 0U; b < kBands; ++b) {
    double energy = 0.0;
    for (size_t k = 0U; k <= kSegment / 2U; ++k) {
      const float f = k * df;
      if (f >= kBandEdgesHz[b] && f < kBandEdgesHz[b + 1U]) {
        energy += static_cast<double>(psd[k]) * scale * df;
      }
    }
    out.band_db[b] = energy > 1e-12 ? 10.0f * log10f(static_cast<float>(energy))
                                    : -100.0f;
  }
  out.valid = true;
  return out;
}

}  // namespace BeeCoolerVib
