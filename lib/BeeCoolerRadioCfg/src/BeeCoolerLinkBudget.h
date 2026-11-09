#ifndef BEE_COOLER_LINK_BUDGET_H
#define BEE_COOLER_LINK_BUDGET_H

// Conta do ar e parametros derivados do enlace LoRa (relatorio AP2, secoes
// 5.1 e 5.5). Sem dependencia de hardware: compila no host para os testes.
//
// O DX-LR32 so expoe perfis fixos (AT+LEVEL). Os dois conhecidos:
//   LEVEL 2 = SF11, 500 kHz, CR 4/5 (AT+HELP: 2149 bps)  -> bancada
//   LEVEL 3 = SF8,  250 kHz, CR 4/5                     -> producao
// Os demais LEVELs nao foram caracterizados: as funcoes devolvem "desconhecido"
// e quem chama escolhe o caso conservador.

#include <stddef.h>
#include <stdint.h>

namespace BeeCoolerLinkBudget {

struct Profile {
  uint8_t sf;
  uint32_t bw_hz;
  uint8_t cr;         // 1 = 4/5 ... 4 = 4/8
  uint16_t preamble;  // simbolos
};

inline bool profileForLevel(int level, Profile& p) {
  switch (level) {
    case 2:
      p = {11U, 500000U, 1U, 8U};
      return true;
    case 3:
      p = {8U, 250000U, 1U, 8U};
      return true;
    default:
      return false;
  }
}

// Tempo no ar (us) pela formula do SX126x: cabecalho explicito, CRC ligado,
// LDRO quando o simbolo passa de 16 ms. Confere com a Tabela 16 do relatorio
// (LEVEL 2: 41 B -> 247 ms, 113 B -> 513 ms; LEVEL 3: 113 B -> 169 ms).
inline uint32_t airtimeUs(const Profile& p, size_t payload_bytes) {
  const uint64_t ts_us = ((1ULL << p.sf) * 1000000ULL) / p.bw_hz;
  const int de = ts_us > 16000ULL ? 1 : 0;
  const int32_t num = 8 * static_cast<int32_t>(payload_bytes) -
                      4 * static_cast<int32_t>(p.sf) + 28 + 16;
  const int32_t den = 4 * (static_cast<int32_t>(p.sf) - 2 * de);
  const int32_t blocks = num > 0 ? (num + den - 1) / den : 0;
  const uint64_t n_payload = 8U + static_cast<uint64_t>(blocks) * (p.cr + 4U);
  // (preambulo + 4,25) simbolos, em quartos de simbolo para ficar inteiro
  const uint64_t preamble_us = (static_cast<uint64_t>(p.preamble) * 4U + 17U) * ts_us / 4U;
  return static_cast<uint32_t>(preamble_us + n_payload * ts_us);
}

// Maior numero de registros por quadro DATA que mantem o tempo no ar dentro
// de `cap_us`. 0 quando o LEVEL e desconhecido.
inline uint8_t maxRecordsPerFrame(int level, uint32_t cap_us, size_t overhead_bytes,
                                  size_t record_bytes, uint8_t hard_max) {
  Profile p;
  if (!profileForLevel(level, p)) return 0U;
  uint8_t best = 0U;
  for (uint8_t n = 1U; n <= hard_max; ++n) {
    if (airtimeUs(p, overhead_bytes + n * record_bytes) <= cap_us) best = n;
  }
  return best;
}

// Espera pelo ACK antes de qualquer medicao no hardware: ACK no ar + 0,42 s
// que a bancada mediu e os calculos nao explicam (5.5) + ~0,28 s de UARTs e
// gravacao no LittleFS da central; tudo +30 %, arredondado para 50 ms.
// LEVEL desconhecido: os 2 s da bancada. O valor medido (console `rtt`)
// substitui este.
inline uint32_t defaultAckTimeoutMs(int level, size_t ack_bytes) {
  Profile p;
  if (!profileForLevel(level, p)) return 2000U;
  const uint32_t base_ms = airtimeUs(p, ack_bytes) / 1000U + 420U + 280U;
  uint32_t t = (base_ms * 13U + 9U) / 10U;
  t = ((t + 49U) / 50U) * 50U;
  return t < 800U ? 800U : t;
}

// p99 + 30 %, arredondado para cima em 50 ms (secao 8.2, passo 2).
inline uint32_t ackTimeoutFromP99Us(uint32_t p99_us) {
  uint32_t t = (p99_us / 1000U) * 13U / 10U + 1U;
  t = ((t + 49U) / 50U) * 50U;
  return t < 300U ? 300U : t;
}

// Ordena `v` em ordem crescente (insercao: n <= 500, sem alocacao) e devolve o
// percentil `pct` (0..100) pelo metodo do posto mais proximo.
inline uint32_t sortAndPercentile(uint32_t* v, size_t n, unsigned pct) {
  if (v == nullptr || n == 0U) return 0U;
  for (size_t i = 1U; i < n; ++i) {
    const uint32_t key = v[i];
    size_t j = i;
    while (j > 0U && v[j - 1U] > key) {
      v[j] = v[j - 1U];
      --j;
    }
    v[j] = key;
  }
  if (pct > 100U) pct = 100U;
  size_t rank = (static_cast<size_t>(pct) * n + 99U) / 100U;  // ceil(p*n/100)
  if (rank == 0U) rank = 1U;
  return v[rank - 1U];
}

// Hora aceitavel do DS1302 ou do ACK: nao anterior a compilacao do firmware e
// nao mais que `max_ahead_s` depois dela (rejeita o 2099 visto na bancada).
inline bool plausibleEpoch(uint32_t epoch, uint32_t build_epoch,
                           uint32_t max_ahead_s) {
  if (build_epoch == 0U) return epoch != 0U;
  if (epoch < build_epoch) return false;
  return epoch - build_epoch <= max_ahead_s;
}

}  // namespace BeeCoolerLinkBudget

#endif  // BEE_COOLER_LINK_BUDGET_H
