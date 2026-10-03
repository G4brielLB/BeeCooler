#pragma once

#include <Arduino.h>

// ADXL345 over SPI (mode 3), FIFO in stream mode with the watermark on INT1
// (Arquitetura v1.2, 4.4). The CPU light-sleeps while the FIFO fills and wakes
// on INT1 to drain it.
struct AdxlWindow {
  bool device_ok;        // DEVID == 0xE5
  bool complete;         // all samples captured
  uint32_t samples;      // per axis
  uint16_t overruns;     // FIFO overrun events seen
  bool activity;         // INT2 activity latched since the last window (D51)
  uint32_t duration_ms;
};

class Adxl345Fifo {
 public:
  void begin(int cs, int int1, int int2);

  // Checks DEVID. Called at every window (RF-14).
  bool probe();

  // Captures `max_samples` samples per axis into `buffer` (interleaved x,y,z
  // int16, 6 bytes per sample), then puts the sensor in standby (or in
  // activity-detection mode when `arm_activity`).
  AdxlWindow capture(int16_t* buffer, uint32_t max_samples,
                     uint32_t timeout_ms, bool arm_activity);

  // Standby without capturing (low-power state between windows).
  void standby();

 private:
  void write(uint8_t reg, uint8_t value);
  uint8_t read(uint8_t reg);
  void readEntry(int16_t* xyz);
  int cs_ = -1, int1_ = -1, int2_ = -1;
};
