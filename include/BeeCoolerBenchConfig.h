#pragma once

#include <stdint.h>

// DX-LR32-900T22D factory UART setting, confirmed for the bench modules.
// This affects ESP <-> LoRa only; the USB Serial monitor stays at 115200.
// Override in BOTH test environments if the modules are reconfigured later.
#ifndef BEE_TEST_LORA_BAUD
#define BEE_TEST_LORA_BAUD 9600U
#endif

namespace BeeCoolerBench {
constexpr uint32_t kLoraBaud = BEE_TEST_LORA_BAUD;
static_assert(kLoraBaud == 2400U || kLoraBaud == 4800U || kLoraBaud == 9600U ||
                  kLoraBaud == 19200U || kLoraBaud == 38400U ||
                  kLoraBaud == 57600U || kLoraBaud == 115200U,
              "Unsupported DX-LR32 UART baud rate");
}  // namespace BeeCoolerBench
