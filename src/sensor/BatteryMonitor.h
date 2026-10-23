#pragma once

#include <Arduino.h>

// Battery voltage through the 0-25 V module on the CN3065 SYS OUT
// (Arquitetura v1.2, 8.2): divider /5 into GPIO4 (ADC1), 6 dB attenuation,
// mean of 32 calibrated readings, gain K_CAL stored in NVS (P-VBAT).
namespace BatteryMonitor {

void begin(int pin);

// Mean of kVbatSamples readings, in mV at the battery. Call at rest, before
// any TX.
uint16_t readMv();

float gain();
void setGain(float k_cal);      // persisted in NVS
float divider();
void setDivider(float divider); // persisted in NVS (module variants, 8.2)

// Calibrates K_CAL so that the current reading equals `measured_mv`.
bool calibrateTo(uint16_t measured_mv);

}  // namespace BatteryMonitor
