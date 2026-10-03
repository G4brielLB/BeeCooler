#pragma once

#include <Arduino.h>

#include <BeeCoolerRecord.h>

// Destination of the RAW vibration windows and extended records
// (Arquitetura v1.2, 9.1). Phase 1 uses SdRawSink; without a card (or in
// Phase 2) NullRawSink discards everything and collection goes on (D50).
class RawSink {
 public:
  virtual ~RawSink() {}
  virtual bool available() const = 0;
  // Appends one window: header + n*(x,y,z) int16 + CRC32 (5.8).
  virtual bool writeWindow(const BeeCoolerRecord::RawHeader& header,
                           const int16_t* samples, size_t n) = 0;
  // Appends one 64-byte extended record (5.9).
  virtual bool writeExtended(const BeeCoolerRecord::ExtendedRecord& record) = 0;
  // Appends one line to events.log.
  virtual bool logEvent(const char* line) = 0;
  // Free space in MB; 0xFFFF when unknown.
  virtual uint16_t freeMegabytes() = 0;
  virtual void end() = 0;
};

class NullRawSink : public RawSink {
 public:
  bool available() const override { return false; }
  bool writeWindow(const BeeCoolerRecord::RawHeader&, const int16_t*,
                   size_t) override { return false; }
  bool writeExtended(const BeeCoolerRecord::ExtendedRecord&) override { return false; }
  bool logEvent(const char*) override { return false; }
  uint16_t freeMegabytes() override { return 0xFFFF; }
  void end() override {}
};

class SdRawSink : public RawSink {
 public:
  // Mounts the card and prepares /BEE/N<node>/<YYYYMMDD>/ for `epoch`. When
  // the card or the directory is missing, available() is false.
  bool begin(int cs, uint8_t node_id, uint32_t epoch);

  bool available() const override { return mounted_; }
  bool writeWindow(const BeeCoolerRecord::RawHeader& header,
                   const int16_t* samples, size_t n) override;
  bool writeExtended(const BeeCoolerRecord::ExtendedRecord& record) override;
  bool logEvent(const char* line) override;
  uint16_t freeMegabytes() override;
  void end() override;

 private:
  bool appendTo(const char* name, const uint8_t* a, size_t na, const uint8_t* b,
                size_t nb, const uint8_t* c, size_t nc);
  bool mounted_ = false;
  char dir_[32] = {};
};
