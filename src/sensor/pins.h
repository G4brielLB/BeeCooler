#pragma once

// Pinagem do no sensor (ESP32-S3 N16R8). 🟡 PROPOSTA: conferir na bancada.
// Guia de ligacao fisica: docs/pinos.md (mantenha os dois em sincronia).
//
// Restricoes que a escolha respeita:
//  - GPIO 26-32 sao da flash e 33-37 da PSRAM octal (N16R8): nao usar.
//  - GPIO 19/20 sao o USB nativo; 0, 3, 45 e 46 sao pinos de strapping.
//  - Apenas GPIO 0-21 sao RTC IO: so eles seguram nivel em deep sleep
//    (gpio_hold). M0/M1 do DX-LR32 precisam disso para o modulo nao sair do
//    sleep enquanto o ESP dorme, entao ficam em 1 e 2.
//  - CS do ADXL345 e do SD flutuam em deep sleep: usar pull-up externo de
//    100 kohm em cada um.

// Bateria: modulo 0-25 V, pino S (v1.2, 8.2)
constexpr int kPinVbat = 4;       // ADC1_CH3
constexpr int kPinCharging = 15;  // CHRG do CN3065 (opcional, -1 desliga)

// SHT30: um por barramento, pois os dois usam o endereco 0x44
constexpr int kPinShtIntSda = 8;
constexpr int kPinShtIntScl = 9;
constexpr int kPinShtExtSda = 17;
constexpr int kPinShtExtScl = 18;

// SPI compartilhado ADXL345 + microSD (FSPI)
constexpr int kPinSpiSck = 12;
constexpr int kPinSpiMosi = 11;
constexpr int kPinSpiMiso = 13;
constexpr int kPinAdxlCs = 10;
constexpr int kPinSdCs = 14;
constexpr int kPinAdxlInt1 = 16;  // watermark do FIFO (v1.2, 4.4)
constexpr int kPinAdxlInt2 = 7;   // atividade (D51)

// DX-LR32 (UART1)
constexpr int kPinLoraTx = 38;   // ESP TX -> RX do modulo
constexpr int kPinLoraRx = 39;   // ESP RX <- TX do modulo
constexpr int kPinLoraM0 = 1;
constexpr int kPinLoraM1 = 2;
constexpr int kPinLoraAux = 41;

// DS1302 (3 fios, bit-bang)
constexpr int kPinRtcCe = 47;
constexpr int kPinRtcIo = 40;
constexpr int kPinRtcSclk = 21;

// ---- Verificacoes em tempo de compilacao
// M0/M1 precisam segurar nivel em deep sleep (gpio_hold): so GPIO RTC (0-21).
static_assert(kPinLoraM0 >= 0 && kPinLoraM0 <= 21 && kPinLoraM1 >= 0 &&
                  kPinLoraM1 <= 21,
              "M0/M1 do DX-LR32 devem estar em GPIO RTC (0-21)");

// (C++11: constexpr recursivo, sem lacos)
namespace pins_check {
constexpr int kAll[] = {kPinVbat,     kPinCharging, kPinShtIntSda, kPinShtIntScl,
                        kPinShtExtSda, kPinShtExtScl, kPinSpiSck,   kPinSpiMosi,
                        kPinSpiMiso,  kPinAdxlCs,   kPinSdCs,      kPinAdxlInt1,
                        kPinAdxlInt2, kPinLoraTx,   kPinLoraRx,    kPinLoraM0,
                        kPinLoraM1,   kPinLoraAux,  kPinRtcCe,     kPinRtcIo,
                        kPinRtcSclk};
constexpr unsigned kCount = sizeof(kAll) / sizeof(kAll[0]);

// kAll[i] nao se repete em kAll[j..]
constexpr bool uniqueFrom(unsigned i, unsigned j) {
  return j >= kCount ||
         ((kAll[i] < 0 || kAll[i] != kAll[j]) && uniqueFrom(i, j + 1));
}
constexpr bool allDistinct(unsigned i = 0) {
  return i >= kCount || (uniqueFrom(i, i + 1) && allDistinct(i + 1));
}

// flash 26-32, PSRAM octal 33-37, USB 19/20, strapping 0/3/45/46
constexpr bool reserved(int p) {
  return (p >= 26 && p <= 37) || p == 19 || p == 20 || p == 0 || p == 3 ||
         p == 45 || p == 46;
}
constexpr bool noReserved(unsigned i = 0) {
  return i >= kCount || (!reserved(kAll[i]) && noReserved(i + 1));
}
}  // namespace pins_check
static_assert(pins_check::allDistinct(), "dois sinais no mesmo GPIO");
static_assert(pins_check::noReserved(),
              "GPIO reservado (flash/PSRAM/USB/strapping) em uso");
