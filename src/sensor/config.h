#pragma once

#include <stdint.h>

// Constantes do firmware do no sensor. Referencias aos documentos:
//   [A] Arquitetura de Software v1.2   [N] Integracao em Nuvem v1.1

#ifndef BEE_NODE_ID
#define BEE_NODE_ID 1  // 1..254 (header LoRa); pode ser sobrescrito na NVS
#endif

constexpr uint16_t kFirmwareVersion = 0x0100;  // 1.0 (RNF-11, vai no HELLO/STATUS)

// ---- Modo de teste acelerado (D53): periodos divididos por 20
#ifdef BEE_TEST_FAST
constexpr uint32_t kTimeDivisor = 20U;  // ciclo 30 s, LoRa a cada 3 min
#else
constexpr uint32_t kTimeDivisor = 1U;   // ciclo 10 min, LoRa a cada 1 h
#endif

// ---- Agenda [A 4.1]
constexpr uint32_t kBootMarginS = 2U;  // acorda antes do slot (resolucao do DS1302 = 1 s)

// ---- Vibracao [A 4.4]
constexpr uint32_t kAdxlOdrHz = 1600U;
constexpr uint32_t kAdxlWindowSamples = 16000U;  // 10 s por eixo
constexpr uint32_t kAdxlWindowTimeoutMs = 13000U;
constexpr uint8_t kAdxlWatermark = 24U;
constexpr uint8_t kAdxlOdrCode = 0x0E;    // BW_RATE
constexpr uint8_t kAdxlRangeCode = 0x00;  // +-2 g, full resolution
constexpr bool kMechEventEnabled = false;  // D51: deteccao de atividade entre janelas

// ---- SHT30 [A 4.3]
constexpr uint8_t kShtReadsPerSensor = 3U;
constexpr uint8_t kShtCommandHi = 0x24;  // single-shot, alta repetibilidade, sem clock stretching
constexpr uint8_t kShtCommandLo = 0x00;
constexpr uint32_t kShtConversionMs = 16U;

// ---- Bateria [A 8.2]
constexpr float kVbatDivider = 5.0f;  // (30k + 7,5k) / 7,5k; ajustar se os resistores diferirem
constexpr uint8_t kVbatSamples = 32U;

// ---- LoRa [A 6.6]
constexpr uint32_t kLoraBaud = 115200U;  // P-UART
constexpr uint32_t kAckTimeoutMs = 400U;  // inicial; fixar em p99 + 30% apos P-RTT
constexpr uint32_t kBackoffMinMs = 50U;
constexpr uint32_t kBackoffMaxMs = 250U;
constexpr uint8_t kMaxDataFramesPerSession = 36U;
constexpr uint32_t kSlotJitterMaxMs = 2000U;

// ---- Relogio [A UC-06]
constexpr uint32_t kClockToleranceS = 2U;
constexpr uint32_t kClockMaxJumpS = 24U * 3600U;

// ---- microSD: -DBEE_NO_SD reproduz a Fase 2 (sem cartao); o mesmo binario
// tambem opera sem cartao em tempo de execucao (D50)
#ifdef BEE_NO_SD
constexpr bool kSdEnabled = false;
#else
constexpr bool kSdEnabled = true;
#endif

// ---- Ring e SD
constexpr const char* kRingPartitionLabel = "beelog";
constexpr uint8_t kRingPartitionSubtype = 0x40;
constexpr uint32_t kSdFreeSpaceRefreshCycles = 144U;
constexpr uint32_t kSdSpiHz = 4000000U;
constexpr uint32_t kAdxlSpiHz = 5000000U;

// ---- Manutencao serial: janela apos o cold boot para entrar no console
constexpr uint32_t kMaintenanceWindowMs = 3000U;
