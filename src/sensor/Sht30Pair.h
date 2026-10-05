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

// Local diagnostics only; never serialized into records or radio packets.
struct ShtDiagnostics {
  bool bus_started = false;
  bool address_found = false;
  uint8_t address = 0;
  uint8_t probe_status[2] = {255, 255};  // 0x44/0x45; 255 = not attempted
  uint8_t command_failures = 0;
  uint8_t read_failures = 0;
  uint8_t decode_failures = 0;
  uint8_t valid_reads = 0;
};

class Sht30Pair {
 public:
  // Probes both sensors; never blocks collection when one is absent (D52).
  void begin(int in_sda, int in_scl, int out_sda, int out_scl);
  ShtResult measure();
  void end();
  const ShtDiagnostics& diagnostics(bool external) const {
    return diagnostics_[external ? 1 : 0];
  }

 private:
  bool probe(TwoWire& bus, uint8_t& address, ShtDiagnostics& diagnostics);
  ShtDiagnostics diagnostics_[2];
  TwoWire* in_bus_ = nullptr;
  TwoWire* out_bus_ = nullptr;
  uint8_t in_addr_ = 0x44;
  uint8_t out_addr_ = 0x44;
  bool in_present_ = false;
  bool out_present_ = false;
};
