#include "BatteryMonitor.h"

#include <Preferences.h>

#include "config.h"

namespace BatteryMonitor {

namespace {

int gPin = -1;
float gGain = 1.0f;
float gDivider = kVbatDivider;
Preferences gPrefs;

void load() {
  if (gPrefs.begin("bee", true)) {
    gGain = gPrefs.getFloat("kcal", 1.0f);
    gDivider = gPrefs.getFloat("vdiv", kVbatDivider);
    gPrefs.end();
  }
  if (!(gGain > 0.5f && gGain < 2.0f)) gGain = 1.0f;
  if (!(gDivider > 1.0f && gDivider < 50.0f)) gDivider = kVbatDivider;
}

}  // namespace

void begin(int pin) {
  gPin = pin;
  analogSetPinAttenuation(gPin, ADC_6db);  // signal <= ~0.85 V: more resolution
  load();
}

uint16_t readMv() {
  uint32_t acc = 0;
  for (uint8_t i = 0; i < kVbatSamples; ++i) acc += analogReadMilliVolts(gPin);
  const float mv = (static_cast<float>(acc) / kVbatSamples) * gDivider * gGain;
  return mv > 65534.0f ? 65534U : static_cast<uint16_t>(mv);
}

float gain() { return gGain; }
float divider() { return gDivider; }

void setGain(float k_cal) {
  gGain = k_cal;
  if (gPrefs.begin("bee", false)) {
    gPrefs.putFloat("kcal", gGain);
    gPrefs.end();
  }
}

void setDivider(float divider) {
  gDivider = divider;
  if (gPrefs.begin("bee", false)) {
    gPrefs.putFloat("vdiv", gDivider);
    gPrefs.end();
  }
}

bool calibrateTo(uint16_t measured_mv) {
  const float previous = gGain;
  gGain = 1.0f;
  const uint16_t raw = readMv();
  gGain = previous;
  if (raw == 0 || measured_mv == 0) return false;
  setGain(static_cast<float>(measured_mv) / raw);
  return true;
}

}  // namespace BatteryMonitor
