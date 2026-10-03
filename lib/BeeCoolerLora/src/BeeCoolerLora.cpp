#include "BeeCoolerLora.h"

namespace {
constexpr uint32_t kModeSettleMs = 2U;  // M0/M1 are sampled by the module
}

bool BeeCoolerLora::begin(HardwareSerial& uart, const Pins& pins,
                          uint32_t baud) {
  if (pins.rx < 0 || pins.tx < 0 || pins.m0 < 0 || pins.m1 < 0 ||
      pins.aux < 0) {
    return false;
  }
  uart_ = &uart;
  pins_ = pins;
  pinMode(pins_.m0, OUTPUT);
  pinMode(pins_.m1, OUTPUT);
  pinMode(pins_.aux, INPUT);
  // Start in sleep so the module does not draw TX/RX current by accident.
  digitalWrite(pins_.m0, HIGH);
  digitalWrite(pins_.m1, HIGH);
  mode_ = Mode::kSleep;
  uart_->begin(baud, SERIAL_8N1, pins_.rx, pins_.tx);
  return true;
}

bool BeeCoolerLora::auxBusy() const {
  return pins_.aux >= 0 && digitalRead(pins_.aux) == HIGH;
}

bool BeeCoolerLora::setMode(Mode mode, uint32_t timeout_ms) {
  if (uart_ == nullptr) return false;
  const uint8_t level = (mode == Mode::kSleep) ? HIGH : LOW;
  digitalWrite(pins_.m0, level);
  digitalWrite(pins_.m1, level);
  mode_ = mode;
  delay(kModeSettleMs);
  const uint32_t start = millis();
  while (auxBusy()) {
    if (millis() - start > timeout_ms) return false;
    delay(1);
  }
  return true;
}

void BeeCoolerLora::flushInput() {
  while (uart_ != nullptr && uart_->available() > 0) uart_->read();
}

bool BeeCoolerLora::send(const uint8_t* data, size_t length,
                         uint32_t& tx_end_us, uint32_t timeout_ms,
                         void (*on_tx_active)(void*), void* context) {
  if (uart_ == nullptr || data == nullptr || length == 0U ||
      mode_ != Mode::kNormal) {
    return false;
  }
  const uint32_t start = millis();
  // Never write into a busy module.
  while (auxBusy()) {
    if (millis() - start > timeout_ms) return false;
    delay(1);
  }
  uart_->write(data, length);
  uart_->flush();  // bytes have left the ESP UART

  // AUX rises when the module starts the radio TX and falls at its end.
  // Allow a short window for the rise; if it never rises the module took the
  // data without raising AUX, which we treat as a failure to transmit.
  const uint32_t rise_deadline = millis() + 100U;
  while (!auxBusy()) {
    if (static_cast<int32_t>(millis() - rise_deadline) > 0) return false;
  }
  if (on_tx_active != nullptr) on_tx_active(context);
  while (auxBusy()) {
    if (millis() - start > timeout_ms) return false;
  }
  tx_end_us = micros();
  return true;
}
