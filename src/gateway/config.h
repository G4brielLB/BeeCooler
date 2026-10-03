#pragma once

#include <stddef.h>
#include <stdint.h>

// Constantes do firmware da central. Referencias:
//   [A] Arquitetura de Software v1.2   [N] Integracao em Nuvem v1.1

constexpr uint16_t kFirmwareVersion = 0x0100;  // 1.0

// ---- LoRa [A 6.6]
constexpr uint32_t kLoraBaud = 115200U;  // P-UART; deve ser igual ao do no
constexpr uint8_t kMaxNodes = 8;         // estado por no mantido em RAM/LittleFS

// ---- Fila de uplink [N 4.2]
constexpr size_t kQueueItemSize = 52U;
constexpr uint32_t kMaxBatchItems = 128U;       // << 32 KB de corpo
constexpr size_t kMaxBodyBytes = 32U * 1024U;
constexpr uint32_t kQueueCompactThreshold = 512U;
constexpr uint32_t kUplinkSettleMs = 5000U;     // espera apos o ultimo ACK de LoRa

// ---- Backoff apos falha de rede / 5xx [N 4.2]: 5 s, 15 s, 60 s, depois 5 min
constexpr uint32_t kBackoffStepsMs[] = {5000U, 15000U, 60000U};
constexpr uint32_t kBackoffSteadyMs = 300000U;

// ---- Heartbeat e relogio [N 4.3, 4.4]
constexpr uint32_t kHeartbeatPeriodMs = 15U * 60U * 1000U;
constexpr uint32_t kNtpResyncMs = 6U * 60U * 60U * 1000U;
constexpr uint32_t kHttpTimeoutMs = 10000U;
constexpr uint32_t kWifiRetryMs = 15000U;

// ---- Tarefas [N 4.1]
constexpr uint32_t kRadioTaskStack = 8192U;
constexpr uint32_t kUplinkTaskStack = 16384U;  // TLS precisa de pilha e heap (P-TLS)
constexpr int kRadioTaskPriority = 5;
constexpr int kUplinkTaskPriority = 1;
constexpr int kRadioTaskCore = 1;
constexpr int kUplinkTaskCore = 0;
