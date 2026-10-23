#include "Ds1302Clock.h"

#include <BeeCoolerLogic.h>
#include <BeeCoolerRecord.h>

namespace {

constexpr uint8_t kRegSeconds = 0x00;
constexpr uint8_t kRegControl = 0x07;
constexpr uint8_t kCmdClockBurstRead = 0xBF;
constexpr uint8_t kCmdClockBurstWrite = 0xBE;
constexpr uint8_t kCmdRamBurstRead = 0xFF;
constexpr uint8_t kCmdRamBurstWrite = 0xFE;
constexpr uint8_t kClockHaltBit = 0x80;

uint8_t fromBcd(uint8_t v) { return static_cast<uint8_t>((v >> 4) * 10 + (v & 0x0F)); }
uint8_t toBcd(uint8_t v) { return static_cast<uint8_t>(((v / 10) << 4) | (v % 10)); }

}  // namespace

void Ds1302Clock::begin(int ce, int io, int sclk) {
  ce_ = ce;
  io_ = io;
  sclk_ = sclk;
  pinMode(ce_, OUTPUT);
  pinMode(sclk_, OUTPUT);
  pinMode(io_, INPUT);
  digitalWrite(ce_, LOW);
  digitalWrite(sclk_, LOW);
}

void Ds1302Clock::start() {
  digitalWrite(sclk_, LOW);
  digitalWrite(ce_, HIGH);
  delayMicroseconds(4);  // tCC
}

void Ds1302Clock::end() {
  digitalWrite(ce_, LOW);
  pinMode(io_, INPUT);
  delayMicroseconds(4);  // tCWH
}

void Ds1302Clock::writeByte(uint8_t value) {
  pinMode(io_, OUTPUT);
  for (uint8_t i = 0; i < 8; ++i) {  // LSB first, data latched on the rising edge
    digitalWrite(io_, (value >> i) & 1U);
    delayMicroseconds(1);
    digitalWrite(sclk_, HIGH);
    delayMicroseconds(1);
    digitalWrite(sclk_, LOW);
    delayMicroseconds(1);
  }
}

uint8_t Ds1302Clock::readByte() {
  pinMode(io_, INPUT);
  uint8_t value = 0;
  for (uint8_t i = 0; i < 8; ++i) {
    // The bit is already on the line after the previous falling edge.
    if (digitalRead(io_)) value |= static_cast<uint8_t>(1U << i);
    digitalWrite(sclk_, HIGH);
    delayMicroseconds(1);
    digitalWrite(sclk_, LOW);
    delayMicroseconds(1);
  }
  return value;
}

uint8_t Ds1302Clock::readRegister(uint8_t reg) {
  start();
  writeByte(static_cast<uint8_t>(0x81 | (reg << 1)));
  const uint8_t v = readByte();
  end();
  return v;
}

void Ds1302Clock::writeRegister(uint8_t reg, uint8_t value) {
  start();
  writeByte(static_cast<uint8_t>(0x80 | (reg << 1)));
  writeByte(value);
  end();
}

void Ds1302Clock::writeProtect(bool on) {
  writeRegister(kRegControl, on ? 0x80 : 0x00);
}

bool Ds1302Clock::halted() {
  return (readRegister(kRegSeconds) & kClockHaltBit) != 0;
}

void Ds1302Clock::readClockRegisters(uint8_t (&r)[8]) {
  start();
  writeByte(kCmdClockBurstRead);
  for (uint8_t i = 0; i < 8; ++i) r[i] = readByte();
  end();
}

bool Ds1302Clock::readEpoch(uint32_t& epoch) {
  uint8_t r[8];
  readClockRegisters(r);

  if (r[0] & kClockHaltBit) return false;
  // 24 h mode only; a 12 h flag means the RTC was never set by this firmware.
  if (r[2] & 0x80) return false;
  const int second = fromBcd(r[0] & 0x7F);
  const int minute = fromBcd(r[1] & 0x7F);
  const int hour = fromBcd(r[2] & 0x3F);
  const int day = fromBcd(r[3] & 0x3F);
  const int month = fromBcd(r[4] & 0x1F);
  const int year = 2000 + fromBcd(r[6]);
  return BeeCoolerLogic::civilToEpoch(year, month, day, hour, minute, second,
                                      epoch);
}

void Ds1302Clock::writeEpoch(uint32_t epoch) {
  int y, mo, d, h, mi, s;
  BeeCoolerLogic::epochToCivil(epoch, y, mo, d, h, mi, s);
  writeProtect(false);
  start();
  writeByte(kCmdClockBurstWrite);
  writeByte(toBcd(static_cast<uint8_t>(s)));  // CH = 0: oscillator runs
  writeByte(toBcd(static_cast<uint8_t>(mi)));
  writeByte(toBcd(static_cast<uint8_t>(h)));
  writeByte(toBcd(static_cast<uint8_t>(d)));
  writeByte(toBcd(static_cast<uint8_t>(mo)));
  writeByte(1);  // day of week, unused
  writeByte(toBcd(static_cast<uint8_t>(y - 2000)));
  writeByte(0x00);  // control register in the burst: write protect off
  end();
  writeProtect(true);
}

bool Ds1302Clock::loadState(DsState& state) {
  uint8_t raw[sizeof(DsState)];
  start();
  writeByte(kCmdRamBurstRead);
  for (size_t i = 0; i < sizeof(raw); ++i) raw[i] = readByte();
  end();
  memcpy(&state, raw, sizeof(state));
  return state.magic == kDsStateMagic && state.layout == kDsStateLayout &&
         state.crc8 == BeeCoolerRecord::crc8(raw, sizeof(raw) - 1);
}

void Ds1302Clock::saveState(DsState& state) {
  state.magic = kDsStateMagic;
  state.layout = kDsStateLayout;
  state.crc8 = BeeCoolerRecord::crc8(reinterpret_cast<const uint8_t*>(&state),
                                     sizeof(state) - 1);
  writeProtect(false);
  start();
  writeByte(kCmdRamBurstWrite);
  const uint8_t* raw = reinterpret_cast<const uint8_t*>(&state);
  for (size_t i = 0; i < sizeof(state); ++i) writeByte(raw[i]);
  end();
  writeProtect(true);
}
