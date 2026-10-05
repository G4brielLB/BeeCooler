#pragma once

// Driver for the DX-LR32-900T22D point-to-point UART module (Arquitetura v1.2,
// section 6). Transparent mode with M0/M1 mode pins and the AUX busy line.
// AUX is high while the module is busy (transmitting / waking up) and low when
// idle, so the end of a TX is the falling edge of AUX (6.6).

#include <Arduino.h>

class BeeCoolerLora {
 public:
  enum class Mode : uint8_t { kNormal, kSleep };

  struct Pins {
    int8_t rx;   // ESP RX (module TX)
    int8_t tx;   // ESP TX (module RX)
    int8_t m0;
    int8_t m1;
    int8_t aux;
  };

  // Returns false only when the pins are invalid; the module is not probed.
  // With SWITCH=0 the module DRIVES M0/M1. Pass false to leave the ESP
  // pins as inputs; only kNormal is supported in this AT-controlled mode.
  // The default preserves existing callers configured for SWITCH=1.
  bool begin(HardwareSerial& uart, const Pins& pins, uint32_t baud,
             bool drive_mode_pins = true);

  // Switches M0/M1 and waits for AUX to go idle (up to `timeout_ms`).
  bool setMode(Mode mode, uint32_t timeout_ms = 1000U);
  Mode mode() const { return mode_; }

  // Writes the frame and returns once AUX has fallen (end of TX). On success
  // `tx_end_us` receives micros() at that instant: the ACK timer starts there.
  // `on_tx_active`, when given, runs once right after AUX rises, i.e. while the
  // radio is transmitting (used to sample the battery under load, 8.2).
  bool send(const uint8_t* data, size_t length, uint32_t& tx_end_us,
            uint32_t timeout_ms = 2000U, void (*on_tx_active)(void*) = nullptr,
            void* context = nullptr);

  int available() { return uart_ != nullptr ? uart_->available() : 0; }
  int read() { return uart_ != nullptr ? uart_->read() : -1; }
  void flushInput();

  bool auxBusy() const;

 private:
  HardwareSerial* uart_ = nullptr;
  Pins pins_ = {-1, -1, -1, -1, -1};
  Mode mode_ = Mode::kSleep;
  bool drive_mode_pins_ = true;
};
