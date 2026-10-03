#pragma once

// Pinagem da central (ESP32-WROOM DevKit + DX-LR32). 🟡 PROPOSTA: conferir.
// Guia de ligacao fisica: docs/pinos.md (mantenha os dois em sincronia).
// UART2 do ESP32 (GPIO16/17) e pinos de uso geral sem restricao de boot.
constexpr int kPinLoraRx = 16;   // ESP RX <- TX do modulo
constexpr int kPinLoraTx = 17;   // ESP TX -> RX do modulo
constexpr int kPinLoraM0 = 25;
constexpr int kPinLoraM1 = 26;
constexpr int kPinLoraAux = 27;

// GPIO 16/17 so estao livres no ESP32-WROOM-32 (no WROVER sao da PSRAM).
static_assert(kPinLoraRx != kPinLoraTx && kPinLoraM0 != kPinLoraM1 &&
                  kPinLoraM0 != kPinLoraAux && kPinLoraM1 != kPinLoraAux &&
                  kPinLoraRx != kPinLoraM0 && kPinLoraRx != kPinLoraM1 &&
                  kPinLoraRx != kPinLoraAux && kPinLoraTx != kPinLoraM0 &&
                  kPinLoraTx != kPinLoraM1 && kPinLoraTx != kPinLoraAux,
              "dois sinais do LoRa no mesmo GPIO");
// 34-39 sao somente entrada; 6-11 sao da flash.
static_assert(kPinLoraTx < 34 && kPinLoraM0 < 34 && kPinLoraM1 < 34 &&
                  (kPinLoraTx < 6 || kPinLoraTx > 11),
              "pino de saida invalido no ESP32");
