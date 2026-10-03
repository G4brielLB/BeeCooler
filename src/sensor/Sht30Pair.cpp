#include "Sht30Pair.h"

#include <Wire.h>

#include <BeeCoolerLogic.h>

#include "config.h"

static_assert(kShtReadsPerSensor == 3, "ShtResult::raw holds 3 reads per sensor");

namespace {

constexpr uint32_t kI2cHz = 100000;
constexpr uint8_t kBytesPerRead = 6;

TwoWire gInBus(0);
TwoWire gOutBus(1);

bool sendCommand(TwoWire& bus, uint8_t address) {
  bus.beginTransmission(address);
  bus.write(kShtCommandHi);
  bus.write(kShtCommandLo);
  return bus.endTransmission() == 0;
}

bool readAnswer(TwoWire& bus, uint8_t address, uint8_t* buffer) {
  if (bus.requestFrom(static_cast<int>(address), static_cast<int>(kBytesPerRead)) !=
      kBytesPerRead) {
    return false;
  }
  for (uint8_t i = 0; i < kBytesPerRead; ++i) buffer[i] = bus.read();
  return true;
}

}  // namespace

bool Sht30Pair::probe(TwoWire& bus, uint8_t& address) {
  static const uint8_t kAddresses[] = {0x44, 0x45};
  for (uint8_t candidate : kAddresses) {
    bus.beginTransmission(candidate);
    if (bus.endTransmission() == 0) {
      address = candidate;
      return true;
    }
  }
  return false;
}

void Sht30Pair::begin(int in_sda, int in_scl, int out_sda, int out_scl) {
  in_bus_ = &gInBus;
  out_bus_ = &gOutBus;
  in_bus_->begin(in_sda, in_scl, kI2cHz);
  out_bus_->begin(out_sda, out_scl, kI2cHz);
  in_present_ = probe(*in_bus_, in_addr_);
  out_present_ = probe(*out_bus_, out_addr_);
}

ShtResult Sht30Pair::measure() {
  using namespace BeeCoolerRecord;
  ShtResult r;
  memset(&r, 0, sizeof(r));
  r.t_in = r.t_out = kInvalidI16;
  r.rh_in = r.rh_out = kInvalidU16;
  for (auto& sensor : r.raw) {
    for (auto& read : sensor) {
      read[0] = static_cast<uint16_t>(kInvalidI16);
      read[1] = kInvalidU16;
    }
  }

  int16_t t[2][kShtReadsPerSensor];
  int16_t rh[2][kShtReadsPerSensor];
  bool ok[2][kShtReadsPerSensor] = {};

  for (uint8_t n = 0; n < kShtReadsPerSensor; ++n) {
    const bool sent_in = in_present_ && sendCommand(*in_bus_, in_addr_);
    const bool sent_out = out_present_ && sendCommand(*out_bus_, out_addr_);
    delay(kShtConversionMs);  // both conversions run in parallel

    uint8_t buffer[kBytesPerRead];
    for (uint8_t s = 0; s < 2; ++s) {
      const bool sent = (s == 0) ? sent_in : sent_out;
      if (!sent) continue;
      TwoWire& bus = (s == 0) ? *in_bus_ : *out_bus_;
      const uint8_t address = (s == 0) ? in_addr_ : out_addr_;
      int16_t tc;
      uint16_t rc;
      if (readAnswer(bus, address, buffer) &&
          BeeCoolerLogic::shtDecode(buffer, tc, rc)) {
        t[s][n] = tc;
        rh[s][n] = static_cast<int16_t>(rc);
        ok[s][n] = true;
        r.raw[s][n][0] = static_cast<uint16_t>(tc);
        r.raw[s][n][1] = rc;
      }
    }
  }

  int16_t out_t, out_rh;
  if (BeeCoolerLogic::medianValid(t[0], ok[0], kShtReadsPerSensor, out_t) &&
      BeeCoolerLogic::medianValid(rh[0], ok[0], kShtReadsPerSensor, out_rh)) {
    r.in_ok = true;
    r.t_in = out_t;
    r.rh_in = static_cast<uint16_t>(out_rh);
  }
  if (BeeCoolerLogic::medianValid(t[1], ok[1], kShtReadsPerSensor, out_t) &&
      BeeCoolerLogic::medianValid(rh[1], ok[1], kShtReadsPerSensor, out_rh)) {
    r.out_ok = true;
    r.t_out = out_t;
    r.rh_out = static_cast<uint16_t>(out_rh);
  }
  return r;
}

void Sht30Pair::end() {
  if (in_bus_ != nullptr) in_bus_->end();
  if (out_bus_ != nullptr) out_bus_->end();
}
