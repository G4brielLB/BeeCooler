#pragma once

#include <Arduino.h>

// DS1302 driver (3-wire bit-bang) with the 31-byte battery-backed RAM block
// of Arquitetura v1.2, section 5.5. Everything here is little-endian.

struct __attribute__((packed)) DsState {
  uint8_t magic;            // 0xBC
  uint8_t layout;           // 1
  uint16_t boot_count;
  uint32_t last_seq;
  uint32_t confirmed_seq;
  uint32_t last_sync_epoch;
  uint8_t power_level;
  uint8_t state_flags;
  uint8_t reserved[12];
  uint8_t crc8;
};
static_assert(sizeof(DsState) == 31, "DS1302 RAM block is 31 bytes");

constexpr uint8_t kDsStateMagic = 0xBC;
constexpr uint8_t kDsStateLayout = 1;

class Ds1302Clock {
 public:
  void begin(int ce, int io, int sclk);

  // True when the oscillator is running and the stored calendar is valid.
  // `epoch` is UTC seconds. False after a CR2032 failure (Clock Halt set).
  bool readEpoch(uint32_t& epoch);
  bool halted();
  void writeEpoch(uint32_t epoch);  // also starts the oscillator

  // RAM block: false when magic/CRC do not validate (state lost).
  bool loadState(DsState& state);
  void saveState(DsState& state);  // fills magic, layout and CRC

 private:
  uint8_t readRegister(uint8_t reg);
  void writeRegister(uint8_t reg, uint8_t value);
  void writeProtect(bool on);
  void start();
  void end();
  void writeByte(uint8_t value);
  uint8_t readByte();

  int ce_ = -1, io_ = -1, sclk_ = -1;
};
