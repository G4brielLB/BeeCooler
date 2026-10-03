#pragma once

#include <Arduino.h>
#include <Wire.h>

#include <BeeCoolerRecord.h>

// The two SHT30 sensors, one per I2C controller (both answer at 0x44).
// Single-shot, high repeatability, no clock stretching; three reads per sensor
// are taken with both sensors converting in parallel (v1.2, 4.3).
struct ShtResult {
  bool in_ok;
  bool out_ok;
  int16_t t_in;     // 0.01 C, median of valid reads
  uint16_t rh_in;   // 0.01 %
  int16_t t_out;
  uint16_t rh_out;
  // Raw reads for rec.bin: [sensor][read][T, RH]; invalid = sentinels.
  uint16_t raw[2][3][2];
};

class Sht30Pair {
 public:
  // Probes both sensors; never blocks collection when one is absent (D52).
  void begin(int in_sda, int in_scl, int out_sda, int out_scl);
  ShtResult measure();
  void end();

 private:
  bool probe(TwoWire& bus, uint8_t& address);
  TwoWire* in_bus_ = nullptr;
  TwoWire* out_bus_ = nullptr;
  uint8_t in_addr_ = 0x44;
  uint8_t out_addr_ = 0x44;
  bool in_present_ = false;
  bool out_present_ = false;
};
