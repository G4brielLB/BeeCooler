# BeeCooler — Arquitetura de Software do Nó Sensor e da Telemetria

**Versão:** 1.2 — 02/10/2026 (v1.1: requisitos, casos de teste e rastreabilidade; v1.2: medição de bateria com o módulo 0–25 V na saída do CN3065)
**Projeto:** BeeCooler — monitoramento e regulação térmica autônoma de colmeias
**Disciplina:** Tópicos em Sistemas Computacionais — UFPI/CCN/DC — 2026.2
**Escopo:** casos de uso, requisitos, casos de teste, ciclo de aquisição, dados persistidos e onde ficam, protocolo LoRa, sincronização, recuperação de falhas, proteção de bateria e transição da Fase 1 (com microSD) para a Fase 2 (sem microSD).
**Documentos relacionados:** Briefing de Decisões v4.1, Revisão de Energia v6, Auditoria Técnica da Fase 0, `beecooler_budget.py` v4.1.
**Status:** documento vivo. `✅` = decisão fechada; `🟡` = proposta ou valor provisório; `🔴` = medição ou validação pendente.

---

## 0. Resumo executivo

A ideia que organiza todo o documento é esta: **a Fase 1 já roda a arquitetura de dados da Fase 2, e o microSD entra como um acessório a mais.**

- Os registros compactos vão para um *ring buffer* na flash interna do ESP32-S3, e é de lá que o LoRa transmite.
- O microSD recebe apenas o RAW de vibração e uma cópia estendida dos registros, para análise.
- Passar para a Fase 2 significa remover o SD. O caminho de telemetria que vai para produção terá rodado em campo durante toda a campanha da Fase 1.

Principais pontos desta versão:

1. Coleta em grade uniforme de **10 min, 24 h**, com T/RH e vibração em todos os ciclos da Fase 1.
2. Agendamento **ancorado no DS1302**, para que a deriva do oscilador interno do S3 não acumule.
3. **Registro canônico de 24 B**, a mesma estrutura na flash, no SD e no LoRa.
4. Protocolo LoRa próprio: *stop-and-wait* por frame, **ACK cumulativo** e confirmação **só após persistência** na central.
5. **Telemetria horária** nas Fases 1 e 2.
6. Relógio sincronizado por **horário embutido em todo ACK**.
7. Backlog recuperado automaticamente quando o enlace volta, **sem perda de dados**.
8. ADXL345 a **1600 Hz** na Fase 1, com base na literatura (seção 10).
9. Proteção de bateria por **firmware com histerese**, antes do corte físico do BMS.
10. **Requisitos, casos de teste e rastreabilidade** no formato da AP1 (seções 13 a 15), voltados ao que a Fase 1 realmente entrega.

Consumo estimado da Fase 1 com essas escolhas: **~51 mAh/dia**, contra ~632 mAh/dia de geração no pior mês médio (margem de ~12,5×). Os valores ainda usam os proxies da v6 até P4c/P4d/P28/P-SD.

---

## 1. Escopo e convenção de fases

Neste documento, as fases seguem o Briefing v4.1 e a Revisão de Energia v6:

| Fase | Armazenamento local | Processamento no nó | Telemetria |
|---|---|---|---|
| **Fase 1** — caracterização (tiúba) | flash interna (ring) + **microSD** (RAW) | features simples; sem TinyML | LoRa, 1 h |
| **Fase 2** — produção / MVP | flash interna (ring) | features + TinyML diurno | LoRa, 1 h + eventos imediatos |

O `hardware.md` usa uma numeração de fases um pouco diferente (LoRa de longo alcance só na Fase 3). A arquitetura descrita aqui não depende dessa numeração: ela é a mesma em bancada, no quintal e no apiário. Muda apenas a distância do enlace.

---

## 2. Decisões desta revisão

| # | Decisão | Status | Revisa | Consequência |
|---|---|---|---|---|
| D39 | Telemetria **a cada 1 h** nas Fases 1 e 2 | ✅ | D29 | Falhas detectadas em até 1 h; custo de ~+0,1 mAh/dia |
| D40 | Fase 1 com grade **uniforme de 10 min, 24 h**, T/RH e vibração em todos os ciclos | 🟡 | D35/D36 (só na Fase 1) | Dataset sem viés dia/noite; firmware sem máquina dia/noite; ~+4,2 mAh/dia |
| D41 | **Ring buffer na flash** como fonte da telemetria já na Fase 1; SD só para RAW e espelho estendido | 🟡 | — | A Fase 2 vira "remover o SD" |
| D42 | **Registro canônico v1 de 24 B** na Fase 1; v2 na Fase 2 | 🟡 | registro de 66 B | 4 registros por frame |
| D43 | Protocolo LoRa próprio, *stop-and-wait*, **ACK cumulativo**, **persistir antes de confirmar** | 🟡 | protocolo da Fase 0 | Corrige descarte de batch e ACK prematuro apontados na auditoria |
| D44 | Sincronização de relógio **pelo ACK** | 🟡 | — | Custo zero de rádio |
| D45 | Agendamento **ancorado no DS1302** | 🟡 | — | Sem deriva acumulada; slots alinhados para TDMA |
| D46 | ADXL345 a **1600 Hz** na Fase 1 | 🟡 | ODR 800 Hz da v6 | Banda útil até 800 Hz; RAW de 96 kB por janela |
| D47 | Features de vibração: remoção de DC, passa-alta ~50 Hz, PSD de Welch com soma vetorial dos 3 eixos, 4 bandas em dB | 🟡 | RMS por eixo | Feature independente da orientação do sensor |
| D48 | Tensão da bateria pelo **módulo sensor de tensão 0–25 V** (divisor 30 kΩ/7,5 kΩ, ÷5) ligado ao **SYS OUT do CN3065**, saída S no GPIO4 (ADC1); divisor de 1 MΩ/1 MΩ + 100 nF fica como alternativa para a Fase 2 | ✅ (Fase 1) | v1.0: divisor 1 MΩ/1 MΩ no P+ do BMS | Montagem simples com módulo já disponível; ~0,1 mA permanentes (~2,5 mAh/dia); precisão de dezenas de mV após calibração |
| D49 | Política de bateria no firmware, **com histerese** | 🟡 | — | O BMS nunca deveria precisar atuar |
| D50 | **Mesmo binário** nas Fases 1 e 2; SD detectado em tempo de execução | 🟡 | — | Falha do SD não derruba a coleta |
| D51 | **Diário de campo obrigatório**; marcação automática de eventos mecânicos pelo INT2 opcional | 🟡 | — | Permite remover eventos não-abelha do dataset |
| D52 | Sensor ausente **não bloqueia** a coleta; o nó coleta o disponível e sinaliza a falha | 🟡 | comportamento da Fase 0 | Campanha longa não perde todos os dados por um único sensor |
| D53 | Firmware com **modo de teste acelerado** (`BEE_TEST_FAST`: ciclo de 30 s, LoRa a cada 3 min) | 🟡 | — | Testes de bancada viáveis em horas, não semanas |

---

## 3. Casos de uso

### 3.1 Atores

| Ator | Papel |
|---|---|
| Temporizador do nó | Dispara cada ciclo, com o calendário ancorado no DS1302 |
| Nó sensor (ESP32-S3) | Coleta, processa, persiste e transmite |
| Central (ESP32-WROOM + DX-LR32) | Recebe, valida, persiste, confirma e encaminha |
| Pesquisador | Visualiza, extrai o SD, mantém o diário de campo |
| Atuador (ESP32-C3) | Executa a nebulização (ramo 2 da Fase 1 / Fase 2) |

### 3.2 Lista

| UC | Caso de uso | Ator / gatilho | Fase |
|---|---|---|---|
| UC-01 | Executar ciclo de aquisição | Temporizador, slot de 10 min | 1, 2 |
| UC-02 | Capturar janela de vibração | Nó, dentro do UC-01 | 1 (RAW), 2 (features) |
| UC-03 | Persistir registro | Nó, ao fim do UC-01 | 1, 2 |
| UC-04 | Enviar telemetria horária | Nó → Central, slot horário do nó | 1, 2 |
| UC-05 | Receber, validar, persistir e confirmar | Central, ao receber frame | 1, 2 |
| UC-06 | Sincronizar relógio | Central, embutido em todo ACK | 1, 2 |
| UC-07 | Recuperar backlog após falha de enlace | Nó, quando o ACK volta | 1, 2 |
| UC-08 | Registrar/reportar evento (reboot, falha de sensor/SD, bateria baixa, sensor encoberto) | Nó | 1 (log + STATUS), 2 (envio imediato) |
| UC-09 | Proteger bateria | Nó, v_bat abaixo do limiar | 1, 2 |
| UC-10 | Recuperar estado após queda de energia | Nó, cold boot | 1, 2 |
| UC-11 | Extrair campanha do SD | Pesquisador, visita/fim de campanha | 1 |
| UC-12 | Visualizar dados e saúde do nó | Pesquisador, dashboard | 1, 2 |
| UC-13 | Acionar nebulização manualmente (teste) | Pesquisador → C3 | 1 (ramo 2) |
| UC-14 | Decidir e acionar resfriamento | Nó (TinyML + regras) → C3 por ESP-NOW | 2 |
| UC-15 | Alterar configuração remotamente | Central, comando embutido no ACK | 2 |
| UC-16 | Anotar intervenções no diário de campo | Pesquisador, a cada visita/evento | 1, 2 |

### 3.3 Detalhamento dos casos centrais

#### UC-06 — Sincronizar relógio

**Problema.** O DS1302 usa um cristal de 32,768 kHz, tipicamente com tolerância da ordem de ±20 ppm a 25 °C e erro crescente longe dessa temperatura. Uma caixa ao sol em Teresina fica boa parte do dia entre 30 e 45 °C, então espera-se deriva de alguns segundos por dia. Isso afeta o slot de transmissão, o futuro TDMA e o cruzamento com dados externos. Se a CR2032 falhar, a hora se perde por completo; o bit *Clock Halt* indica isso no boot.

**Fluxo.**

1. A central obtém UTC por NTP (Wi-Fi).
2. Todo ACK carrega o epoch da central.
3. O nó compara com o DS1302. Se `|Δ| > 2 s`, regrava o DS1302 e marca `clock_adjusted` no próximo registro.
4. O atraso do ACK (~100 ms) é desprezível, porque o DS1302 tem resolução de 1 s.

**Regras.**

- Rejeitar horários absurdos: anteriores à data de compilação do firmware, ou com salto de mais de 24 h quando o relógio local é válido.
- Nunca reescrever registros antigos. A ordem é dada pelo `seq`; a central corrige timestamps offline se necessário.
- Cold boot com hora inválida: o nó envia HELLO imediatamente. Sem resposta, coleta com a flag `rtc_invalid`, e a central reconstrói os timestamps a partir do `seq` e do primeiro horário válido.

#### UC-07 — Recuperar backlog

**Cenário.** A central fica indisponível (queda de energia, PC desligado, Wi-Fi fora) ou o enlace falha (chuva, obstrução).

**No nó, durante a falha.**

- A coleta continua e os registros vão para o ring da flash.
- A cada hora, o nó tenta o STATUS. Sem ACK depois de 4 tentativas, aborta a sessão.
- O ponteiro `confirmed_seq` (RAM do DS1302) não avança.

**Quando o enlace volta.**

- O campo `acked_up_to` do ACK informa o maior `seq` contíguo **já persistido** na central.
- O nó envia a partir de `acked_up_to + 1`, do mais antigo para o mais novo, com no máximo **36 frames por sessão** (144 registros, um dia de dados).
- Exemplo: 2 dias de queda geram ~288 registros pendentes, drenados em ~3 sessões horárias. O dashboard já mostra o estado atual desde a primeira sessão, porque o STATUS leva o registro mais recente.

**A central é a autoridade sobre o que tem.**

- Se o nó perder o ponteiro (CR2032), o próximo `acked_up_to` o reposiciona.
- Se a central perder dados, o `acked_up_to` volta para trás e o nó reenvia, porque o ring guarda anos de registros.
- Reenvios de algo que a central já tem são descartados pela deduplicação por `(node_id, seq)`.

O limite de 36 frames (~0,5 mAh, ~6 s de airtime) evita rajadas longas de rádio, o que importa para a energia e para P14.

#### UC-09 — Proteger bateria

Ver seção 8. É uma máquina de estados no firmware (`PowerPolicy`). O BMS 1S é a última barreira física e não deve ser o mecanismo normal de desligamento.

#### UC-10 — Recuperar estado após queda de energia

No cold boot, o nó:

1. lê o bloco de estado na RAM do DS1302 (seção 5.5) e valida o CRC8;
2. confere na flash o slot do último `seq` (CRC16 + `seq` coincidente);
3. se o DS1302 perdeu o conteúdo, varre os cabeçalhos dos setores da partição para achar o maior `seq` válido;
4. incrementa `boot_count` e envia HELLO com o motivo do reset.

O `seq` continua crescendo depois do reboot. Isso corrige o problema da Fase 0, em que IDs reiniciavam em 1 e não havia `boot_id`.

---

## 4. Ciclo de aquisição (UC-01/UC-02)

### 4.1 Agendamento ancorado no DS1302

O timer de deep sleep do S3 usa o oscilador RC interno, que deriva com a temperatura. Se cada ciclo simplesmente dormir 600 s, o calendário escorrega. O DS1302 não tem saída de alarme e não consegue acordar o ESP.

Por isso, a cada wake:

```text
agora        = DS1302
proximo_slot = próximo múltiplo de 10 min (hh:00, hh:10, ...)
dormir       = proximo_slot − agora − margem_de_boot
```

O erro não acumula, e os slots ficam alinhados entre nós.

### 4.2 Linha do tempo de um wake (Fase 1)

| Etapa | Duração aprox. | Estado | Observação |
|---|---:|---|---|
| Boot do deep sleep | ~0,25 s | ativo | restaura contexto da RTC RAM |
| DS1302 + ADC da bateria (média de 32 amostras) | ~5 ms | ativo | sempre antes de qualquer TX |
| SHT30 int + ext, 3 leituras cada | ~50 ms | ativo | conversões **em paralelo**, um sensor em cada barramento I²C |
| Janela ADXL345, 10 s a 1600 Hz | 10 s | light sleep na maior parte | CPU acorda pelo INT1 (watermark), drena o FIFO, volta a dormir |
| Processamento (DC, passa-alta, Welch, bandas) | ~0,1 s 🔴 | processamento | medir em P4d |
| SD: montar, append RAW + registro estendido, `fsync`, desmontar | 0,3–0,6 s 🔴 | SD ativo | P-SD3 |
| Append no ring da flash + RAM do DS1302 | poucos ms | ativo | |
| Sessão LoRa (só no slot horário) | ~1,5–2 s | rádio | seção 6 |
| ADXL em standby/atividade, rádio em sleep, deep sleep | — | — | |

Tempo acordado: **~11–12 s por ciclo** (~14 s no ciclo com LoRa). O deep sleep ocupa ~98% do tempo.

A ordem é proposital: o rádio nunca transmite durante a janela do ADXL nem durante a escrita no SD. Isso evita somar picos de corrente no TPS63020 e contaminar a vibração com ruído de alimentação.

### 4.3 SHT30

| Parâmetro | Valor |
|---|---|
| Modo | *single-shot*, alta repetibilidade, sem *clock stretching* (comando `0x2400`, ~15 ms máx.) |
| Leituras | 3 por sensor, os dois sensores em paralelo |
| Validação | CRC8 de cada palavra; faixa física (−40 a 125 °C; 0 a 100% RH) |
| Valor no registro | **mediana** das leituras válidas |
| Valor no SD | as 3 leituras brutas de cada sensor |
| Proibido | modo periódico (consome continuamente) |

**Detecção de sensor encoberto.** Meliponíneos usam cerume e geoprópolis em abundância, e a literatura relata sensores dentro da colmeia cobertos por própolis (seção 10). Se a variância da T/RH interna nas últimas 6 h for praticamente nula enquanto a externa varia, o nó marca `sht_in_stuck`. O histórico necessário (36 registros) fica na RTC RAM.

### 4.4 ADXL345

| Parâmetro | Valor | Registro (a conferir no datasheet) 🟡 |
|---|---|---|
| Interface | SPI modo 3, até 5 MHz | — |
| Faixa | ±2 g, *full resolution* (3,9 mg/LSB) | `DATA_FORMAT = 0x08` |
| ODR | **1600 Hz** (banda útil até 800 Hz) | `BW_RATE = 0x0E` |
| FIFO | modo *stream*, watermark 24 | `FIFO_CTL = 0x98` |
| Interrupções | watermark → INT1 (GPIO16); atividade → INT2 (GPIO7) | `INT_MAP`, `INT_ENABLE` |
| Janela | 10 s = 16.000 amostras/eixo = **96 kB** | buffer na SRAM interna |
| Overrun | bit de overrun em `INT_SOURCE` → flag `fifo_overrun` | — |
| Entre janelas | standby (~0,1 µA) **ou** modo de atividade de baixo consumo (opcional, D51) | `POWER_CTL` |

**Por que watermark 24 e não 32.** Os 8 níveis de folga absorvem a latência de saída do light sleep. A 1600 Hz, o FIFO atinge 24 amostras a cada 15 ms.

**Campanha opcional de banda larga** 🟡: 1 ou 2 dias com uma janela por hora a 3200 Hz (`BW_RATE = 0x0F`), para verificar se a tiúba tem conteúdo entre 800 e 1600 Hz. Nesse modo, o buffer da janela (192 kB) deve ir para a PSRAM.

**Marcação de eventos mecânicos (opcional, D51).** Entre janelas, o ADXL pode ficar em modo de baixo consumo com detecção de atividade, na faixa de dezenas de µA. O evento fica latched no sensor e é lido no próximo wake, virando a flag `mech_event`, sem acordar o ESP a cada pancada. Custo estimado de ~1 mAh/dia. Serve para marcar abertura da caixa, choques e vandalismo.

### 4.5 Processamento da vibração no nó

```text
RAW x,y,z (int16)
  → remove média por eixo
  → passa-alta ~50 Hz (IIR de 2ª ordem)       # modos estruturais da caixa
  → RMS total  = sqrt(rms_x² + rms_y² + rms_z²)
  → pico       = max |a − média| (módulo)
  → Welch: Hann de 512 pontos, 50% de overlap  # ~3,1 Hz/bin, ~61 segmentos
  → PSD_total = PSD_x + PSD_y + PSD_z          # independente da orientação
  → energia em 4 bandas → 10·log10 → dB
```

| Banda | Motivação (literatura para *Apis*, provisória para tiúba) |
|---|---|
| 50–100 Hz | sinais de baixa frequência associados ao nível de atividade |
| 100–200 Hz | região da frequência fundamental observada (~120–165 Hz) |
| 200–350 Hz | segundo pico principal; *quacking*, *tooting*, *waggle* |
| 350–600 Hz | *whooping*, *tremble*, *piping* |

O RAW no SD permite refazer toda essa escolha depois. Na Fase 1, as features do nó também servem de **teste do firmware**: ao fim da campanha, elas são recalculadas no computador a partir do RAW e comparadas com o que o nó calculou.

### 4.6 Tensão da bateria

Ver seção 8.2.

---

## 5. Dados: o que é salvo e onde

### 5.1 Princípio

Uma única estrutura, o **registro canônico**, vale para a flash, o SD e o LoRa. Isso dá uma única fonte da verdade e um único parser. Todos os campos multibyte são **little-endian** (nativo do ESP32). Valores inválidos usam as sentinelas já adotadas na Fase 0: `INT16_MIN` para campos com sinal e `UINT16_MAX` (ou `0xFF` em campos de 1 B) para campos sem sinal.

### 5.2 Registro canônico v1 (24 B)

```c
typedef struct __attribute__((packed)) {
  uint32_t ts;          // epoch UTC (DS1302)
  int16_t  t_in;        // 0,01 °C
  uint16_t rh_in;       // 0,01 %
  int16_t  t_out;       // 0,01 °C
  uint16_t rh_out;      // 0,01 %
  uint16_t vbat_mv;     // mV, medida em repouso
  uint16_t vib_rms;     // 0,1 mg, após remoção de DC e passa-alta
  uint16_t vib_peak;    // 0,1 mg
  uint8_t  band_db[4];  // 50–100 / 100–200 / 200–350 / 350–600 Hz
  uint16_t flags;       // ver 5.3
} bee_rec_v1_t;         // 24 B — o seq NÃO entra aqui
```

- `band_db`: `valor = round(2 × (dB + 100))`, ou seja, passos de 0,5 dB a partir de −100 dB; `0xFF` = inválido. 🟡 A referência de dB (por exemplo, (mg)²/Hz) será fixada com os primeiros dados.
- O `seq` não fica no registro: vai no slot da flash e no header do frame, onde os registros de um frame são contíguos.

### 5.3 Flags

| Bit | Nome | Significado |
|---:|---|---|
| 0 | `sht_in_fail` | sem leitura válida do SHT30 interno |
| 1 | `sht_out_fail` | sem leitura válida do SHT30 externo |
| 2 | `adxl_fail` | DEVID inválido ou erro de SPI |
| 3 | `fifo_overrun` | overrun do FIFO durante a janela |
| 4 | `rtc_invalid` | DS1302 sem hora válida |
| 5 | `clock_adjusted` | relógio corrigido pelo ACK desde o último registro |
| 6 | `cold_boot` | primeiro registro após reset |
| 7 | `sd_fail` | escrita no SD falhou neste ciclo |
| 8 | `vib_skipped` | janela de vibração não executada (política de energia ou erro) |
| 9–10 | `power_level` | 0 normal, 1 economia, 2 crítico, 3 hibernação |
| 11 | `sht_in_stuck` | possível sensor interno encoberto |
| 12 | `mech_event` | atividade mecânica detectada desde o último ciclo (INT2) |
| 13 | `charging` | CN3065 carregando (se CHRG estiver ligado a um GPIO) |
| 14–15 | reservado | — |

### 5.4 Mapa de memória

| Local | Conteúdo | Sobrevive a | Fase |
|---|---|---|---|
| RTC RAM do S3 (~8 KB) | `seq` atual, ponteiros do ring, últimos 36 registros (6 h), estado do agendador, contadores de TX, offset de relógio | deep sleep | 1, 2 |
| RAM do DS1302 (31 B) | último `seq`, `confirmed_seq`, `boot_count`, nível de energia | perda da 18650 (CR2032) | 1, 2 |
| Flash — partição `beelog` (8 MB, P32) | ring de slots de 32 B | tudo | 1, 2 |
| Flash — NVS | configuração, `node_id`, calibração do ADC | tudo | 1, 2 |
| SRAM interna | buffer RAW de 96 kB da janela | — (volátil) | 1, 2 |
| PSRAM (8 MB) | FFT, buffers grandes, tensor arena; RAW a 3200 Hz | — (volátil) | 1 (opcional), 2 |
| microSD 16 GB | RAW, registro estendido, log de eventos | tudo | **1** |

### 5.5 Bloco de estado na RAM do DS1302 (31 B)

| Offset | Campo | B |
|---:|---|---:|
| 0 | magic `0xBC` | 1 |
| 1 | versão do layout | 1 |
| 2 | `boot_count` | 2 |
| 4 | `last_seq` | 4 |
| 8 | `confirmed_seq` | 4 |
| 12 | `last_sync_epoch` | 4 |
| 16 | `power_level` | 1 |
| 17 | flags de estado | 1 |
| 18 | reservado | 12 |
| 30 | CRC8 do bloco | 1 |

Escrever na RAM do DS1302 não desgasta a flash. Por isso, o `confirmed_seq` pode ser atualizado a cada ACK.

### 5.6 Ring buffer na flash

```text
slot (32 B) = [seq u32][bee_rec_v1_t 24 B][CRC16 2 B][marker 0xA55A 2 B]

N_slots = 8 MB / 32 B = 262.144   (~5 anos a 144 registros/dia)
slot    = seq mod N_slots
setor   = 4 KB = 128 slots → apagado imediatamente antes de receber o primeiro slot
```

- **Escrita:** quando `seq mod 128 == 0`, apaga o setor seguinte e depois grava. Um apagamento a cada ~21 h; desgaste desprezível.
- **Leitura para LoRa:** posição calculada direto do `seq`, sem busca.
- **Recuperação (UC-10):** confere o slot de `last_seq` (CRC16 + `seq` coincidente). Se a RAM do DS1302 estiver inválida, lê o primeiro slot de cada um dos 2.048 setores e localiza o maior `seq` válido.

### 5.7 Organização do microSD (Fase 1)

```text
/BEE/N01/20261005/            ← um diretório por dia (UTC)
    raw.bin      janelas de vibração (formato 5.8)
    rec.bin      registro estendido (formato 5.9)
    events.log   texto: boot e motivo, erros, resumo de cada sessão LoRa
```

Cada wake executa `open(append) → write → fsync → close`. Nenhum arquivo fica aberto durante o sono. Se o SD falhar, o nó **continua normalmente** com flash e LoRa e marca `sd_fail`. O STATUS horário informa espaço livre e erros do cartão, então dá para saber se o SD está gravando sem ir à colmeia.

### 5.8 Formato da janela RAW (`raw.bin`)

| Offset | Campo | B |
|---:|---|---:|
| 0 | magic `"BEEV"` | 4 |
| 4 | versão | 1 |
| 5 | `node_id` | 1 |
| 6 | código do ODR | 1 |
| 7 | código da faixa (±g) | 1 |
| 8 | `seq` (liga a janela ao registro) | 4 |
| 12 | `ts` início | 4 |
| 16 | `n_samples` por eixo | 2 |
| 18 | `fifo_overruns` | 2 |
| 20 | `t_in` no momento da janela (0,01 °C) | 2 |
| 22 | flags | 2 |
| 24 | reservado | 8 |
| 32 | payload: `n_samples × (x, y, z)` em int16 | 6·n |
| fim | CRC32 do header + payload | 4 |

O ADXL345 não tem sensor de temperatura interno; o campo `t_in` vem do SHT30 interno e serve para avaliar a dependência térmica da sensibilidade. Formato binário, e não CSV: menos bytes, menos tempo de escrita, menos energia, parser simples em Python.

### 5.9 Registro estendido (`rec.bin`, 64 B)

| Campo | B |
|---|---:|
| `seq` | 4 |
| registro canônico v1 | 24 |
| 3 leituras brutas de T e RH de cada SHT30 (6 × 2 × 2 B) | 24 |
| duração do wake (ms) | 2 |
| tempo de escrita no SD (ms) | 2 |
| tempo de processamento (ms) | 2 |
| `fifo_overruns` | 1 |
| vbat sob carga no último TX (mV) | 2 |
| reservado + CRC16 | 3 |

Os campos de diagnóstico alimentam P4d e P-SD3 com dados reais de campo, sem instrumentação extra.

### 5.10 Volumes

| Item | Fase 1 |
|---|---:|
| Registros/dia | 144 |
| Ring (canônico) | ~4,6 KB/dia |
| RAW a 1600 Hz | ~13,8 MB/dia |
| RAW em 15 dias | ~207 MB |
| Registro estendido | ~9 KB/dia |

Capacidade não é problema para o cartão de 16 GB.

---

## 6. Protocolo LoRa (UC-04/UC-05/UC-07)

### 6.1 Por que um protocolo próprio

O DX-LR32-900T22D é um módulo ponto a ponto via UART/AT, sem pilha LoRaWAN, e a central não é um gateway LoRaWAN. Ele entrega bytes pela UART sem garantia de enquadramento. O frame, portanto, precisa de byte de sincronização, comprimento e CRC. O CRC32 reaproveita a implementação da Fase 0 (ISO-HDLC, polinômio `0xEDB88320`).

Esta é a **versão 2** do protocolo, incompatível com a da Fase 0.

### 6.2 Header comum (13 B) + CRC32 (4 B) = 17 B de overhead

| Offset | Campo | B | Função |
|---:|---|---:|---|
| 0 | magic `0xBE` | 1 | sincronização no fluxo UART |
| 1 | `len` | 1 | comprimento total do frame |
| 2 | `ver_tipo` | 1 | 4 bits de versão + 4 bits de tipo |
| 3 | `node_id` | 1 | até 254 nós |
| 4 | `boot_id` | 1 | `boot_count mod 256` |
| 5 | `frame_seq` | 2 | identidade do frame; retransmissões reutilizam |
| 7 | `attempt` | 1 | número da tentativa (1–4) |
| 8 | `first_seq` | 4 | `seq` do primeiro registro |
| 12 | `n_rec` | 1 | registros contíguos no frame |
| … | payload | ≤ 111 | |
| fim | CRC32 | 4 | cobre header + payload |

O campo `attempt` permite que a central meça retransmissões reais. Era a métrica que faltou para avaliar o RNF-01 na AP1.

### 6.3 Tipos de frame

| Código | Tipo | Sentido | Payload | Frame total |
|---:|---|---|---|---:|
| `0x1` | DATA | nó → central | até 4 registros v1 (96 B) | ≤ 113 B |
| `0x2` | STATUS | nó → central | registro mais recente + saúde (36 B) | 53 B |
| `0x3` | HELLO | nó → central | boot (10 B) | 27 B |
| `0x4` | EVENT | nó → central | tipo + registro (Fase 2) | 42 B |
| `0x8` | ACK | central → nó | ver 6.4 | 21 B |

**STATUS (36 B):** registro v1 mais recente (24 B; seu `seq` vai em `first_seq`), backlog pendente (2 B), espaço livre no SD em MB (2 B), erros de SD (1 B), motivo do último reset (1 B), RSSI do último ACK (1 B), sessões falhas consecutivas (1 B), vbat sob carga (2 B), versão do firmware (2 B).

**HELLO (10 B):** versão do firmware (2), motivo do reset (1), RTC válido (1), `last_seq` (4), `boot_count` (2).

### 6.4 ACK (21 B, layout próprio)

| Campo | B |
|---|---:|
| magic, `len`, `ver_tipo`, `node_id` | 4 |
| `frame_seq` ecoado | 2 |
| `acked_up_to` (maior `seq` contíguo persistido na central) | 4 |
| epoch UTC da central | 4 |
| RSSI do frame recebido (`AT+DRSSI`) | 1 |
| SNR do frame recebido | 1 |
| comando pendente (UC-15, Fase 2; `0` = nenhum) | 1 |
| CRC32 | 4 |

### 6.5 Sessão horária

```text
nó, após persistir o registro de hh:00
 │ espera o slot: t_tx = hh:00 + 20 s + (node_id − 1) × 30 s + jitter(0–2 s)
 │   (light sleep até o slot; na Fase 1 com um nó, são ~8 s)
 │ rádio: M0/M1 → normal, espera AUX baixo
 │
 │ TX STATUS ──────────────► central
 │ ◄──────────────────────── ACK (acked_up_to, epoch, RSSI)
 │    sem ACK após 4 tentativas → ABORTA a sessão (dados ficam no ring)
 │
 │ ACK ok → UC-06: corrige DS1302 se |Δt| > 2 s
 │ TX DATA a partir de acked_up_to + 1, do mais antigo para o mais novo,
 │    até 36 frames por sessão
 │    cada ACK → grava confirmed_seq na RAM do DS1302
 │
 │ rádio: M0/M1 = HIGH/HIGH (sleep) → deep sleep
```

Em regime normal são 6 registros por hora: STATUS + 2 DATA, ~72 frames/dia, ~1,2 mAh/dia.

### 6.6 Temporização, retransmissão e abort

| Parâmetro | Valor | Status |
|---|---|---|
| LEVEL do DX-LR32 | 3 (SF8, 250 kHz) como referência | 🔴 P8/P14 |
| Airtime DATA (113 B) / ACK (21 B) | ~171 ms / ~53 ms | 🔴 P-HDR |
| Início do cronômetro do ACK | **quando o AUX do nó cai** (fim do TX), não quando a escrita na UART retorna | ✅ |
| Timeout inicial | 400 ms | 🟡 |
| Timeout final | p99 do RTT medido + 30% (esperado ~250–300 ms) | 🔴 P-RTT |
| Tentativas por frame | 4 (1 + 3 retransmissões) | 🟡 |
| Backoff entre tentativas | aleatório, 50–250 ms | 🟡 |
| Falha de um frame | **aborta a sessão inteira** | ✅ |
| Máximo de DATA por sessão | 36 frames | 🟡 |
| UART ESP ↔ módulo | 115200 baud, se estável | 🔴 P-UART |

Abortar a sessão quando um frame esgota as tentativas corrige o maior desperdício da Fase 0, em que o nó continuava tentando todos os fragmentos com a central inalcançável.

**Orçamento do RTT esperado (115200 baud):** UART do DATA na central (~10 ms) + validação e persistência (~10–30 ms) + UART, airtime e UART do ACK (~57 ms) + latências internas do módulo (🔴) ≈ 80–100 ms + módulo. A 9600 baud, o total sobe para ~230–250 ms + módulo.

### 6.7 Deduplicação e idempotência

- Retransmissões reutilizam `frame_seq` e `first_seq`.
- A central insere por `(node_id, seq)` de forma idempotente. Duplicatas recebem novo ACK e não geram registro novo.
- O `acked_up_to` cumulativo resolve qualquer desacordo entre nó e central (seção 3.3, UC-07).

### 6.8 Considerações regulatórias

Rajadas de backlog (até 36 frames, ~6 s de airtime) devem entrar na análise de P14 (dwell/LBT/FHSS). O módulo oferece LBT (`AT+LBT`), que pode ser habilitado se a regra aplicável exigir.

---

## 7. Central (UC-05)

### 7.1 Persistir antes de confirmar

Na Fase 0, o ACK saía antes da remontagem e da persistência. Na arquitetura nova, a central:

1. valida magic, `len` e CRC32;
2. insere de forma idempotente por `(node_id, seq)`;
3. persiste numa fila LittleFS na flash do WROOM;
4. atualiza `acked_up_to` do nó (maior `seq` contíguo persistido);
5. **só então** envia o ACK;
6. encaminha ao PC/servidor com confirmação própria (*store-and-forward* em dois saltos).

### 7.2 Estado por nó (NVS/LittleFS da central)

`acked_up_to`, último `boot_id` visto, último RSSI/SNR, histograma de `attempt`, horário do último frame.

### 7.3 Destino dos dados

| Fase | Caminho | Armazenamento |
|---|---|---|
| 1 | central → serial → coletor Python da Fase 0, atualizado | **SQLite** com `UNIQUE(node_id, seq)` no lugar dos CSVs |
| 2 | central → Wi-Fi → MQTT/HTTP | servidor do dashboard |

A central obtém o horário por NTP e o repassa em cada ACK. Ela também registra RSSI, SNR e tentativas por frame, que alimentam diretamente a avaliação de confiabilidade do enlace.

---

## 8. Energia e proteção de bateria (UC-09)

### 8.1 Duas camadas de proteção

| Camada | Quem | Quando atua | O que faz |
|---|---|---|---|
| Lógica | firmware (`PowerPolicy`) | limiares abaixo (P33) | reduz atividade aos poucos e avisa a central |
| Física | BMS 1S 3 A | ~2,5–3,0 V, conforme a placa | corta a carga; última barreira |

### 8.2 Medição da tensão

**Componente.** Módulo sensor de tensão DC "0–25 V". Na prática, é um divisor resistivo de **30 kΩ / 7,5 kΩ** que divide a tensão por 5. A faixa nominal de 25 V vale para ADC de 5 V; no ESP32-S3 (ADC até ~3,3 V), a faixa útil é ~16 V, sobrando muito para a 18650. Com a célula cheia (4,2 V), o pino S entrega **~0,84 V** ao GPIO.

Antes de instalar, conferir os resistores com o multímetro, com o módulo desconectado: VCC→S deve dar ~30 kΩ e S→GND ~7,5 kΩ. Há variantes no mercado; se os valores forem outros, ajustar a constante `DIV` do firmware.

**Ponto de medição: SYS OUT do CN3065.** Não é possível levar fio do P+ do BMS direto ao borne do módulo, então a leitura é feita na saída do carregador. Pela topologia do briefing (seção 2.1), esse ponto mede essencialmente a tensão da célula:

```text
18650 ── B+/B− ── BMS ── P+/P− ── BATT IN │ CN3065 │ SYS OUT ──┬── TPS63020 ── 3,3 V
                                                                │
                                         borne VCC do módulo ◄──┘ (SYS OUT +)
                                         borne GND do módulo ◄──── SYS OUT − (GND do sistema)

                                         pino S ──► GPIO4 (ADC1_CH3)
                                         pino − ──► GND do ESP32-S3
                                         pino + ──► não usar
```

Consequências desse ponto:

- Mede a tensão que **o TPS63020 realmente recebe**, ou seja, a tensão que alimenta o nó.
- Na maioria das placas CN3065, SYS OUT é ligado diretamente a BATT IN. Se a placa tiver diodo ou MOSFET no caminho, aparece uma pequena queda que varia com a corrente. **Confirmar com o multímetro** a diferença entre BATT IN e SYS OUT, em repouso e durante uma transmissão LoRa (P-VBAT).
- Se o BMS cortar, a leitura vai a zero, mas nesse caso o nó também está desligado.
- O terra do módulo, o SYS OUT −, o P− do BMS e o GND do ESP precisam ser o mesmo nó elétrico.

**Firmware.**

```cpp
const int   PIN_VBAT = 4;
const float DIV      = 5.0f;    // (30k + 7,5k) / 7,5k — ajustar se os resistores diferirem
float       K_CAL    = 1.000f;  // ganho de calibração, salvo na NVS (P-VBAT)

void vbatInit() {
  analogSetPinAttenuation(PIN_VBAT, ADC_6db);  // sinal ≤ ~0,85 V: atenuação baixa = mais resolução
}

uint16_t vbatReadMv() {
  uint32_t acc = 0;
  for (int i = 0; i < 32; i++) acc += analogReadMilliVolts(PIN_VBAT);  // mV calibrados (eFuse)
  return (uint16_t)((acc / 32.0f) * DIV * K_CAL);
}
```

- Média de 32 amostras, **em repouso, antes de qualquer TX**.
- Na primeira transmissão da sessão, uma segunda leitura sob carga. A diferença entre as duas indica a resistência interna da célula e vai no STATUS como diagnóstico de envelhecimento.
- Calibração (P-VBAT): medir a célula com o multímetro, calcular `K_CAL = V_multímetro / V_lido` e salvar na NVS; repetir em outro nível de carga para confirmar.

**Precisão esperada.** O divisor ÷5 multiplica por 5 o erro do ADC: ~10 mV no pino viram ~50 mV na bateria. Sem calibração, espera-se algo em torno de ±50 mV; com o `K_CAL`, algumas dezenas de mV. É suficiente para a política de energia:

| Tensão em repouso | Situação aproximada |
|---|---|
| ≥ 4,1 V | cheia |
| ~3,7–3,9 V | meio da carga |
| ≤ 3,5 V | baixa (zona de economia, seção 8.3) |

**Consumo.** O divisor fica permanentemente ligado à célula: **~0,1 mA** (4,2 V / 37,5 kΩ ≈ 112 µA na célula cheia), cerca de **2,5 mAh/dia**, já incluídos na seção 8.4. Na medição de deep sleep (P4c), esse valor aparece somado ao consumo do nó e deve ser informado separadamente.

**Cuidados de interpretação.**

- Com sol, o carregador eleva a tensão em SYS OUT e a leitura diurna superestima a carga. As decisões usam a **mediana das últimas 3 leituras em repouso**, com preferência para as noturnas.
- A curva da Li-ion é plana entre ~3,6 e 3,9 V. Pequenos erros de ADC parecem grandes variações de carga; por isso a calibração importa.
- Opcional: ligar o status de carga do CN3065 (CHRG, normalmente ligado ao LED da placa) a um GPIO livre (1, 2, 5 ou 6). O nó passa a saber se está carregando (flag `charging`), o que ajuda a interpretar a tensão e valida a geração solar real.

**Fase 2.** Se o consumo de ~0,1 mA pesar depois das medições de P4c, o módulo pode ser substituído por um divisor de 1 MΩ/1 MΩ com 100 nF no GPIO4 (~2 µA). O firmware muda só a constante `DIV` e a atenuação.

**Atuador.** O briefing já prevê o mesmo tipo de módulo para a SLA no ESP32-C3. A 12–14,4 V, o pino S fica em ~2,4–2,9 V; com atenuação de 12 dB, dá para ler, o que atende o bloqueio por bateria baixa do RF-16.

### 8.3 Níveis de energia (placeholders até P33)

| Nível | Entra abaixo de | Sai acima de | Comportamento |
|---|---:|---:|---|
| 0 — Normal | — | — | ciclo completo |
| 1 — Economia | 3,45 V | 3,60 V | sem vibração; LoRa a cada 4 h |
| 2 — Crítico | 3,35 V | 3,50 V | só T/RH a cada 30 min; um STATUS por dia |
| 3 — Hibernação | 3,25 V | 3,45 V | acorda a cada 2 h só para medir a tensão; sem rádio |

- Mudança de nível exige **3 leituras consecutivas** no mesmo sentido.
- A **histerese** (limiar de saída acima do de entrada) evita que o nó oscile entre modos.
- Toda mudança de nível vai no campo `power_level` dos registros e é reportada no próximo STATUS.
- Os limiares serão ajustados depois de medir a curva real da célula e a leitura do ADC.

### 8.4 Orçamento energético estimado — Fase 1

Base: Revisão de Energia v6 (Fase 1 com 92 wakes/dia, 79 janelas RAW a 800 Hz e LoRa a cada 4 h), com os mesmos proxies (DevKit 1,24 mA em deep sleep, 46 mA ativa, 4,3 mA em light sleep; SD 0,17 mA idle / 210 mA ativo / 0,5 s por wake).

| Parcela | mAh/dia |
|---|---:|
| Fase 1 da v6 | 40,66 |
| Grade uniforme de 10 min (D40): +52 wakes de T/RH, +65 janelas RAW | +4,23 |
| Telemetria horária (D39) em vez de 4 h | +0,10 |
| ODR 1600 Hz (D46): janela e escrita no SD maiores | +3,1 🟡 |
| Módulo de tensão 0–25 V permanentemente ligado (D48, ~0,1 mA) | +2,5 |
| **Total Fase 1** | **~50,6** |
| Opcional: detecção de atividade entre janelas (D51) | +~1 |

| Indicador | Sem D51 | Com D51 |
|---|---:|---:|
| Margem solar no pior mês (631,8 mAh/dia) | ~12,5× | ~12,2× |
| Autonomia sem sol (2210 mAh úteis) | ~44 dias | ~43 dias |

Esses números continuam sendo **análise de sensibilidade**, não autonomia medida. P4c, P4d, P28 e P-SD substituem os proxies.

---

## 9. Transição da Fase 1 para a Fase 2

### 9.1 Mesmo binário, detecção em tempo de execução

```text
RecordStore   → FlashRing                                (Fases 1 e 2)
RawSink       → SdRawSink | NullRawSink | FlashSnapshotSink
Link          → LoraLink (protocolo da seção 6)
Clock         → Ds1302Clock + correção pelo ACK
Sensors       → Sht30Pair, Adxl345Fifo
Features      → VibFeatures (DC, passa-alta, Welch, bandas)
Scheduler     → cálculo de slot (+ regime dia/noite na Fase 2)
PowerPolicy   → níveis de bateria (seção 8)
```

No boot, o firmware tenta montar o SD. Se conseguir, `RawSink = SdRawSink`; se não, `NullRawSink`. Assim, "funciona na Fase 1 mas quebra na Fase 2" deixa de ser possível, e uma falha do SD no meio da campanha não derruba a coleta.

### 9.2 O que muda

| Aspecto | Fase 1 | Fase 2 |
|---|---|---|
| Cadência | 10 min, 24 h, tudo | 10 min de dia com vibração e TinyML; T/RH a cada 30 min à noite (v6) |
| RAW | SD, toda janela | descartado após extrair features |
| Registro | v1, 24 B | v2: + bandas escolhidas na Fase 1, estado/duração da bomba, saída do modelo |
| Persistência | ring + espelho estendido no SD | ring |
| LoRa | 1 h | 1 h + EVENT imediato (atuação, alarme) |
| Extração de dados | cartão | dump do ring via USB em modo manutenção |
| Configuração remota | — | comando no ACK (UC-15) |

### 9.3 RAW na Fase 2 sem SD (opcional) 🟡

Uma partição de ~2 MB na flash pode guardar cerca de 20 janelas a 1600 Hz como *snapshots* de eventos: anomalia de vibração, início de atuação ou pedido remoto.

- Recuperação por USB no modo manutenção.
- Ou, raramente, por LoRa: uma janela de 96 kB custa ~870 frames, ~150 s de airtime e ~13 mAh. Inviável como fluxo contínuo, aceitável como envio raro sob demanda, sujeito a P14.

---

## 10. Contribuições da literatura

### 10.1 Trabalhos analisados

| | Rigakis et al., *Sensors* 2023 | Książek et al., *Appl. Sci.* 2025 |
|---|---|---|
| Espécie | *Apis mellifera* | *A. m. carnica* |
| Sensor de vibração | transdutor piezoelétrico com guia de onda metálico, amplificado e filtrado | acelerômetro **ADXL355** (baixo ruído) sobre os quadros de cria |
| Amostragem | 8 kHz, 12 bits | 4 kHz |
| Janela / período | 5 s a cada 5 min | 20 s a cada minuto |
| Features | energia em 4 bandas (0–100, 200–350, 300–450, 400–600 Hz) | PSD de Welch (janela Hann, 50% overlap), soma vetorial dos 3 eixos, log; seleção de bins por RFECV |
| Plataforma | STM32L4 + LTE, SD, painel 20 W, bateria 12 Ah | ESP32-WROOM + SD + RTC, alimentação de rede |
| Transmissão | horária, com plano configurável pelo servidor | — (dados no SD) |

### 10.2 O que muda no BeeCooler

| Achado | Consequência no projeto |
|---|---|
| A informação útil para classificar o ciclo anual da colônia está **abaixo de ~1 kHz**; os autores sugerem que ~2 kHz de amostragem bastaria | **D46:** ODR de 1600 Hz (banda até 800 Hz); campanha opcional a 3200 Hz |
| Picos principais em 100–200 Hz e 200–300 Hz; fundamental ~120–165 Hz | Bandas 100–200 e 200–350 Hz no registro (D47) |
| Sinais de comunicação de *Apis* entre ~300 e 600 Hz (*whooping*, *tremble*, *piping*) | Banda 350–600 Hz; com 800 Hz de ODR, metade dessa região se perderia |
| O sucesso usou o **ADXL355**, bem menos ruidoso que o ADXL345 | **P6 sobe para prioridade alta** (SNR caixa vazia × colônia) |
| O modelo de identificação de colônia aprendeu **características da caixa e do sensor**, não das abelhas | Montagem rígida, posição fixa durante toda a campanha, horas de **caixa vazia** como referência, passa-alta ~50 Hz |
| Soma vetorial dos espectros dos 3 eixos | Feature independente da orientação (D47) |
| Eventos não-abelha (roçagem, inspeções, trabalho na caixa) removidos antes do treino | **Diário de campo obrigatório** e flag `mech_event` opcional (D51) |
| Formato binário próprio com flag de overflow do FIFO e temperatura | Já alinhado ao `raw.bin`; `t_in` acrescentado ao header |
| Barramentos SPI separados para SD e acelerômetro, porque gravavam durante a aquisição | No BeeCooler a janela fica em RAM e só é gravada depois; o SPI compartilhado do briefing continua válido |
| Sensor de gás coberto por própolis, detectado pelos valores anômalos | Detecção de `sht_in_stuck` no firmware |
| Amostragem uniforme de 5 min e envio horário mantidos no inverno com folga energética | Reforça D39 e D40 |
| Limiares de alerta usados (8 °C / 37 °C / 90% RH) | Válidos para *Apis*; **não** se aplicam à tiúba (P9) |

### 10.3 Limite de transferência

Os dois trabalhos são com *Apis mellifera*, em caixas, clima e sensores diferentes dos nossos. As bandas e a frequência de amostragem adotadas aqui são **pontos de partida**. A Fase 1 existe justamente para medir o que vale para a tiúba, e o RAW no SD é o que permite refazer essas escolhas sem nova campanha.

---

## 11. Diário de campo (UC-16)

Cada intervenção ou evento externo é anotado num CSV compartilhado:

```text
inicio_utc,fim_utc,tipo,descricao,responsavel
2026-10-05T12:10:00Z,2026-10-05T12:25:00Z,inspecao,abertura do ninho para foto,Fulano
```

Tipos sugeridos: `inspecao`, `manutencao`, `rocagem`, `chuva_forte`, `alimentacao`, `troca_bateria`, `troca_sd`, `outro`. Na análise, esses intervalos são removidos ou rotulados antes de qualquer treino. A flag `mech_event` (se D51 for adotada) ajuda a localizar eventos esquecidos no diário.

---

## 12. Pendências

### 12.1 Criadas por esta arquitetura

| ID | O que é | Por que importa | Como medir | O que decide |
|---|---|---|---|---|
| **P-RTT** | Tempo entre o fim do TX do nó (AUX cai) e a chegada completa do ACK | Timeout curto gera retransmissões à toa; longo deixa o nó acordado à toa quando a central está fora | 500 trocas em bancada, `esp_timer` (µs) no nó, persistência da central ligada | Timeout = p99 + 30% |
| **P-UART** | Baud da UART entre ESP e DX-LR32 | A 9600 baud, só o frame de 113 B leva ~118 ms no fio, nos dois lados | Configurar 115200 pelo comando AT do manual; ~10.000 frames contando erros de CRC | Baud de produção e boa parte do RTT |
| **P-HDR** | Se o módulo acrescenta bytes ao pacote e como agrupa os bytes da UART | Bytes extras aumentam o airtime; agrupamento por tempo ocioso pode **partir o frame em dois** na central | Duração do AUX alto para frames de 20/64/113/128 B vs. fórmula; verificar se cada frame chega num único bloco | Tamanho máximo real do frame; robustez do parser |
| **P-SD3** (+ P-SD1/P-SD2) | Tempo e corrente de montar, gravar, `fsync` e fechar por wake, agora com 96 kB | Maior incerteza energética da Fase 1; cartões têm pausas internas ocasionais de centenas de ms | Timestamps por etapa em centenas de ciclos + corrente por shunt/INA219; olhar o pior caso | Custo real da Fase 1; pior tempo acordado |
| **P-ODR** | Confirmar 1600 Hz e, se feita, a campanha a 3200 Hz | Define banda útil, volume de RAW e custo da janela | Comparar espectros e taxa de overrun nas duas configurações | ODR de produção |
| **P-VBAT** | Calibração do módulo de tensão 0–25 V e verificação do ponto de medição (SYS OUT do CN3065) | O divisor ÷5 amplifica o erro do ADC e a curva plana da Li-ion amplifica o efeito disso; uma eventual queda entre BATT IN e SYS OUT entra direto na leitura | Conferir os resistores do módulo; comparar a leitura com o multímetro em 3–4 tensões; medir BATT IN − SYS OUT em repouso e durante TX | `DIV` e `K_CAL` na NVS; confiança nos limiares de P33 |

### 12.2 Pendências existentes afetadas

| ID | Mudança |
|---|---|
| **P6** | Prioridade **alta**: SNR caixa vazia × colônia, agora com o ADXL345 a 1600 Hz |
| P4c / P4d | Também devem medir light sleep com watermark a cada 15 ms e o processamento de Welch |
| P11 | Passa a ser respondida pela análise do RAW a 1600 Hz (e 3200 Hz, se houver) |
| P14 | Incluir as rajadas de backlog de até 36 frames |
| P32 | Partição `beelog` de 8 MB confirmada como alvo; avaliar partição de *snapshots* de 2 MB para a Fase 2 |
| P33 | Limiares da seção 8.3 |
| P9 | Limiares de alerta da tiúba independem dos valores de *Apis* da literatura |

---

## 13. Requisitos e critérios de aceitação

Esta seção segue o formato do Capítulo 2 da AP1. A numeração **reinicia neste ciclo** (Ciclo de Projeto 2 / Fase 1); a correspondência com os requisitos da AP1 está na seção 15.2.

Os requisitos foram escritos para o que este ciclo de fato entrega: o nó completo com energia própria, uma colônia de tiúba no quintal, 7 a 15 dias de coleta, LoRa a curta distância e o atuador validado em bancada. Os requisitos marcados como **Fase 2** já orientam o firmware, mas só serão verificados no ciclo seguinte. O alcance de ~300 m no apiário **não** é requisito deste ciclo (seção 13.3).

### 13.1 Requisitos funcionais

| ID | O sistema deve… | Critério de aceitação | Fase |
|---|---|---|---|
| RF-01 | Ler a temperatura e a umidade relativa interna e externa a cada 10 min, em slots alinhados ao relógio | Em qualquer período de 24 h de operação normal, há 144 registros, um por slot, com `ts` a até ±5 s do início do slot; leituras válidas permanecem nas faixas físicas (−40 a 125 °C; 0 a 100% RH); leituras com falha aparecem com sentinela e flag, nunca como valor plausível | 1, 2 |
| RF-02 | Capturar, a cada ciclo, uma janela de vibração de 10 s nos três eixos do ADXL345 a 1600 Hz, usando o FIFO | Cada janela do `raw.bin` tem 16.000 amostras por eixo e o mesmo `seq` do registro correspondente; no máximo 1% das janelas apresenta overrun, sempre sinalizado; em repouso, a magnitude média é 1,00 ± 0,10 g; uma batida na estrutura produz aumento observável do RMS em relação ao repouso | 1, 2 |
| RF-03 | Calcular no nó as features de vibração (RMS, pico e 4 bandas em dB) | Recalculadas no computador a partir do RAW, as features coincidem com as do nó em ≥ 95% das janelas, com tolerância de ±2% no RMS e de ±1 dB em cada banda | 1, 2 |
| RF-04 | Persistir cada registro na flash interna antes de voltar a dormir | Após 1.000 ciclos em modo de teste, o ring contém `seq` contíguos com CRC válido, inclusive nas trocas de setor; em 20 cortes de energia em instantes aleatórios, perde-se no máximo o registro do ciclo em andamento | 1, 2 |
| RF-05 | Gravar no microSD o RAW de vibração e o registro estendido, sem depender do cartão para continuar operando | Os arquivos diários são lidos pelo script de análise com CRC válido; ao remover o cartão durante a operação, coleta e telemetria continuam e a flag `sd_fail` aparece a partir do ciclo seguinte, sem reinicialização | 1 |
| RF-06 | Manter horário de calendário pelo DS1302 e corrigi-lo pela hora da central | O horário sobrevive a resets do ESP e à remoção da 18650 (com a CR2032); um desvio forçado de 60 s é corrigido na sessão seguinte para \|Δ\| ≤ 2 s, com a flag `clock_adjusted`; horários absurdos enviados pela central são rejeitados | 1, 2 |
| RF-07 | Transmitir telemetria por LoRa a cada 1 h, com confirmação por ACK e até 4 tentativas por frame | Em bancada, 24 sessões em 24 h; nenhum frame com mais de 4 tentativas (campo `attempt`); com a central desligada, a sessão é abortada após 4 tentativas do STATUS e os ciclos de 10 min seguintes ocorrem normalmente, sem travamento | 1, 2 |
| RF-08 | Reenviar automaticamente os registros não confirmados quando o enlace volta | Com a central desligada por 12 h (72 registros pendentes), após religá-la todos os registros chegam em até 2 sessões, sem duplicatas e sem lacunas de `seq` no banco | 1, 2 |
| RF-09 | Na central, validar a integridade dos frames, descartar duplicatas e só confirmar o que já foi persistido | Frames com CRC corrompido não geram registro nem ACK; frames duplicados recebem novo ACK sem gerar registro; se a central for reiniciada entre a recepção e a persistência, o nó não recebe ACK, retransmite e nenhum registro se perde | 1, 2 |
| RF-10 | Registrar no host os dados recebidos e os logs de comunicação para análise posterior | 100% dos registros aceitos ficam no SQLite com `UNIQUE(node_id, seq)`; cada execução do coletor mantém seu log bruto; os dados podem ser exportados em CSV | 1, 2 |
| RF-11 | Mostrar no dashboard as séries de T/RH interna e externa, as features de vibração, a tensão da bateria e o estado do enlace | O dashboard apresenta as séries e, para o nó, último contato, RSSI, backlog e nível de energia, com atualização da interface a cada 2 s; sem contato há mais de 70 min, o nó aparece como silencioso | 1, 2 |
| RF-12 | Medir a tensão da bateria e reduzir a atividade do nó por níveis, com histerese | Após calibração, erro ≤ 50 mV na faixa de 3,0 a 4,2 V em relação ao multímetro nos terminais da célula; com fonte ajustável no lugar da bateria, cada limiar da seção 8.3 muda o nível após 3 leituras consecutivas, a volta respeita a histerese e o nível aparece nos registros e no STATUS | 1, 2 |
| RF-13 | Retomar a operação após reset ou queda de energia sem reiniciar a numeração dos registros | Em 20 cortes de energia, o `seq` nunca se repete nem volta a 1, o `boot_count` aumenta em 1 a cada boot, a central recebe o HELLO e o primeiro registro tem a flag `cold_boot` | 1, 2 |
| RF-14 | Detectar falhas de sensor em operação sem publicar valores aparentemente válidos | Ao desconectar cada sensor com o nó em operação, o ciclo seguinte traz sentinela e flag correspondente; ao reconectar, a leitura volta em até 2 ciclos sem reinicialização; o DEVID do ADXL345 é conferido a cada janela | 1, 2 |
| RF-15 | No atuador, acionar a bomba por comando ESP-NOW durante um tempo definido e registrar cada acionamento | Para comandos de 30, 60 e 120 s, a bomba liga e desliga com erro ≤ 1 s; o atuador confirma o comando ao emissor; nunca ultrapassa a duração máxima absoluta (🟡 300 s), mesmo sem novo comando; após reset do C3, a bomba inicia desligada; cada acionamento fica registrado com horário, duração e motivo | 1 (ramo 2), 2 |
| RF-16 | No atuador, impedir o acionamento com reservatório vazio ou bateria SLA abaixo do limiar | Com a chave-boia indicando vazio, ou com a SLA abaixo do limiar (🟡 a definir, ex. 11,8 V em repouso), o comando é recusado e o motivo é devolvido ao emissor | 1 (ramo 2), 2 |
| RF-17 | Decidir na borda o acionamento do resfriamento e enviar o comando ao atuador | Em bancada, com T_in forçada acima do limiar, o comando sai no mesmo ciclo; as regras de segurança (duração máxima, intervalo mínimo entre acionamentos, sem acionamento noturno) nunca são violadas; cada decisão fica registrada com o motivo | 2 |
| RF-18 | Operar sem microSD com o mesmo firmware da Fase 1 | Sem cartão, todos os requisitos funcionais, exceto o RF-05, continuam atendidos | 2 |
| RF-19 | Enviar evento imediato à central no início e no fim de cada acionamento | Com enlace disponível, os eventos chegam à central no mesmo ciclo em que ocorreram | 2 |

### 13.2 Requisitos não funcionais

| ID | Atributo | Valor alvo | Como se verifica |
|---|---|---|---|
| RNF-01 | Completude da coleta no nó | ≥ 98% dos slots de 10 min da campanha com registro válido no ring, descontados os intervalos anotados no diário de campo | CT-07 |
| RNF-02 | Completude da entrega à central | ≥ 99% dos registros do ring presentes no banco ao fim da campanha | CT-07 |
| RNF-03 | Qualidade do enlace no quintal | ≥ 90% dos frames confirmados na primeira tentativa, na distância do quintal (registrada no relatório) | CT-07 |
| RNF-04 | Autonomia energética | ≥ 7 dias contínuos com painel solar e 18650, sem recarga manual, sem reinicialização não planejada e sem entrar no nível Crítico | CT-07 |
| RNF-05 | Consumo em deep sleep do nó completo (P4c) | ≤ 4 mA no lado da 18650, incluindo o módulo de tensão | CT-06 |
| RNF-06 | Consumo diário | ≤ 100 mAh/dia, calculado com P4c, P4d, P28 e P-SD medidos (margem ≥ 6× no pior mês) | CT-06 |
| RNF-07 | Tempo acordado por ciclo | p95 ≤ 15 s em ciclo normal e ≤ 20 s em ciclo com sessão LoRa | CT-07 (`rec.bin`) |
| RNF-08 | Exatidão do timestamp | \|Δ\| ≤ 5 s em relação à central em ≥ 99% das sessões | CT-07 (log da central) |
| RNF-09 | Integridade dos dados armazenados | ≥ 99,9% das janelas RAW e dos registros com CRC válido | CT-02 e CT-07 |
| RNF-10 | Custo do nó sensor (referência de produção, sem microSD) | ≤ R$ 400,00 | Lista de materiais (Briefing v4.1, seção 10.1) |
| RNF-11 | Rastreabilidade da versão | 100% dos registros associados à versão de firmware que os gerou; dependências do PlatformIO e do coletor fixadas por versão | Verificação documental + HELLO/STATUS |

### 13.3 Restrições

O cenário e os recursos deste ciclo impõem as seguintes restrições:

- **Energia.** O nó sensor opera exclusivamente com painel de 1,25 W, CN3065, BMS 1S, 18650 de 2600 mAh e TPS63020, com um único trilho de 3,3 V. O atuador usa a SLA de 12 V com recarga manual.
- **Rádio.** O DX-LR32 é controlado apenas por UART/AT, com os LEVELs predefinidos do fabricante. O enquadramento regulatório do modo escolhido (P14) ainda está aberto, o que exige parâmetros conservadores de airtime.
- **Local e alcance.** A campanha ocorre no quintal de um dos integrantes, com uma colônia de tiúba. Como já previsto na AP1 (seção 7.2), o alcance de ~300 m no apiário será validado no ciclo seguinte; aqui o LoRa é caracterizado em bancada e usado a curta distância.
- **Duração.** A campanha tem de 7 a 15 dias, uma única colônia e uma única caixa. Isso limita qualquer conclusão estatística sobre o comportamento da espécie.
- **Armazenamento.** O microSD existe apenas neste ciclo; nenhum requisito da Fase 2 pode depender dele.
- **Instrumentação.** As medições de corrente dependem dos instrumentos disponíveis à equipe (multímetro e, se disponível, módulo INA219 ou shunt com osciloscópio).

---

## 14. Verificação e testes

### 14.1 Modo de teste acelerado

Esperar 10 min por ciclo tornaria os testes de bancada inviáveis. O firmware terá uma opção de compilação `BEE_TEST_FAST`:

| Parâmetro | Produção | Modo de teste |
|---|---|---|
| Período do ciclo | 10 min | 30 s |
| Sessão LoRa | a cada 1 h | a cada 3 min (6 ciclos) |
| Demais comportamentos | — | idênticos |

Com isso, 1.000 ciclos levam ~8 h, e as trocas de setor do ring (a cada 128 registros) acontecem várias vezes num único ensaio. Os registros produzidos nesse modo levam uma flag de teste no `events.log` e nunca entram no dataset.

### 14.2 Visão geral

| ID | Requisitos verificados | Objetivo da verificação | Ambiente | Evidências |
|---|---|---|---|---|
| CT-01 | RF-01, RF-02, RF-03, RF-14 | Aquisição, features e tratamento de falhas de sensor | bancada | `raw.bin`, `rec.bin`, saída do script de comparação |
| CT-02 | RF-04, RF-05, RF-13, RNF-09 | Persistência e recuperação após queda de energia | bancada, modo de teste | dump do ring, arquivos do SD, log da central |
| CT-03 | RF-07, RF-09 | Protocolo LoRa: ACK, retransmissão, abort, duplicatas, CRC | bancada | logs do nó e da central, SQLite |
| CT-04 | RF-06, RF-08 | Sincronização de relógio e recuperação de backlog | bancada, modo de teste | logs, SQLite |
| CT-05 | RF-12 | Medição da bateria e política de energia | bancada, fonte ajustável | planilha de calibração, registros |
| CT-06 | RNF-05, RNF-06 | Caracterização energética do nó (P4c, P4d, P28, P-SD) | bancada | planilha de medições, calculadora atualizada |
| CT-07 | RF-10, RF-11, RNF-01 a RNF-04, RNF-07 a RNF-09 | Campanha de coleta com a tiúba | quintal, 7–15 dias | SQLite, SD, diário de campo, capturas do dashboard |
| CT-08 | RF-15, RF-16 | Atuador em bancada | bancada | log do C3, cronometragem, vídeo |
| CT-09 | RF-17, RF-19 | Integração nó–atuador e decisão na borda | bancada/quintal | **Fase 2** |
| CT-10 | RF-18 | Firmware sem microSD | bancada | **Fase 2** |
| Lista de materiais | RNF-10 | Custo do nó sensor | — | Briefing v4.1, seção 10.1 |
| Verificação de versão | RNF-11 | Versão do firmware rastreável | — | `platformio.ini`, tag do repositório, HELLO/STATUS |

### 14.3 Procedimentos

#### CT-01 — Aquisição, features e falhas de sensor

| Campo | Conteúdo |
|---|---|
| Requisitos | RF-01, RF-02, RF-03, RF-14 |
| Objetivo | Verificar a leitura dos sensores, o alinhamento dos slots, a captura da janela de vibração, as features calculadas no nó e o comportamento diante de sensores desconectados |
| Material | Nó sensor completo alimentado pela 18650; central ligada ao notebook; script de análise do repositório; nível de bolha |

**Procedimento**

1. Gravar o firmware de produção (período de 10 min) com a tag de versão anotada.
2. Apoiar o nó numa superfície nivelada e deixá-lo em repouso por 24 h.
3. Durante esse período, em três momentos anotados, dar uma batida leve na estrutura logo após o início de um slot.
4. Num outro momento anotado, desconectar o SHT30 interno por 2 ciclos e reconectar; repetir com o SHT30 externo e com o ADXL345.
5. Ao fim, extrair o SD e rodar o script, que: conta registros por slot e mede o desvio de `ts`; confere amostras por janela e overruns; calcula a magnitude média nas janelas de repouso; recalcula as features a partir do RAW e compara com as do nó.

**Resultado esperado**

- 144 registros em 24 h, com `ts` a até ±5 s do início do slot.
- 16.000 amostras por eixo em cada janela; overrun em no máximo 1% das janelas.
- Magnitude média de 1,00 ± 0,10 g em repouso; RMS visivelmente maior nas janelas com batida.
- Features do nó dentro da tolerância do RF-03 em ≥ 95% das janelas.
- Sentinela e flag no ciclo seguinte a cada desconexão; leitura restabelecida em até 2 ciclos após reconectar, sem reinicialização.

| Resultado obtido | Veredito | Evidência |
|---|---|---|
| — | A executar | `ensaios/ct01/` |

#### CT-02 — Persistência e recuperação após queda de energia

| Campo | Conteúdo |
|---|---|
| Requisitos | RF-04, RF-05, RF-13, RNF-09 |
| Objetivo | Verificar que nenhum registro já persistido se perde em quedas de energia, que a numeração continua após o boot e que a falha do SD não interrompe a operação |
| Material | Nó em modo de teste, alimentado por fonte com chave liga/desliga em série; central e coletor ativos; script de dump do ring via USB |

**Procedimento**

1. Gravar o firmware em modo de teste e deixar rodar até completar 1.000 ciclos (~8 h).
2. Fazer um dump do ring e verificar `seq` contíguos e CRC válido em todos os slots, inclusive nas trocas de setor.
3. Realizar 20 cortes de energia em instantes escolhidos ao acaso (incluindo durante a janela de vibração, a escrita no SD e a sessão LoRa), religando após alguns segundos.
4. Após cada religamento, registrar o `seq` do primeiro registro, o `boot_count` e o HELLO recebido pela central.
5. Com o nó em operação, remover o microSD por 10 ciclos e recolocá-lo; depois, reiniciar o nó com o cartão.
6. Validar com o script todos os arquivos do SD e todos os slots do ring.

**Resultado esperado**

- `seq` contíguos e CRC válido nos 1.000 ciclos.
- Em cada corte, perda de no máximo o registro do ciclo em andamento; `seq` nunca repetido nem reiniciado; `boot_count` +1; HELLO recebido; flag `cold_boot` no primeiro registro.
- Com o cartão removido: coleta e telemetria continuam e a flag `sd_fail` aparece a partir do ciclo seguinte, sem reinicialização.
- ≥ 99,9% das janelas e registros com CRC válido; corrupção só na janela interrompida pelo corte.

| Resultado obtido | Veredito | Evidência |
|---|---|---|
| — | A executar | `ensaios/ct02/` |

#### CT-03 — Protocolo LoRa em bancada

| Campo | Conteúdo |
|---|---|
| Requisitos | RF-07, RF-09 |
| Objetivo | Verificar ACK, limite de tentativas, abort de sessão, rejeição de frames corrompidos, deduplicação e confirmação somente após persistência; medir o RTT para fixar o timeout |
| Material | Nó e central com DX-LR32 a poucos metros; notebook com coletor; firmware da central com opções de teste para corromper o CRC e para reiniciar entre recepção e persistência |

**Procedimento**

1. **P-UART e P-HDR:** configurar 115200 baud nos dois módulos; enviar ~10.000 frames de 20, 64, 113 e 128 B, contando erros de CRC e verificando se cada frame chega num único bloco; medir o tempo de AUX alto e comparar com o airtime calculado.
2. **P-RTT:** com o timeout inicial de 400 ms, realizar 500 trocas e registrar o RTT no nó; calcular p50, p99 e máximo e fixar o timeout em p99 + 30%.
3. Rodar 24 h em produção (sessões horárias) e conferir no log da central o campo `attempt` de todos os frames.
4. Desligar a central e observar duas sessões do nó: número de tentativas do STATUS e continuidade dos ciclos de 10 min.
5. Ativar na central a corrupção proposital de 1 em cada 10 frames recebidos e verificar a ausência de registro e de ACK para esses frames.
6. Descartar propositalmente na central o envio de alguns ACKs e verificar no banco a ausência de duplicatas após as retransmissões.
7. Ativar a opção de reiniciar a central entre recepção e persistência e verificar que o nó retransmite e o registro chega ao banco.

**Resultado esperado**

- 115200 baud sem erros de CRC; frames inteiros em um único bloco; airtime medido compatível com o calculado.
- Timeout definido a partir do RTT medido.
- 24 sessões em 24 h; nenhum frame com mais de 4 tentativas.
- Com a central desligada, abort após 4 tentativas do STATUS e ciclos de 10 min intactos.
- Nenhum registro ou ACK para frames corrompidos; nenhuma duplicata no banco; nenhum registro perdido no reinício da central.

| Resultado obtido | Veredito | Evidência |
|---|---|---|
| — | A executar | `ensaios/ct03/` |

#### CT-04 — Sincronização de relógio e recuperação de backlog

| Campo | Conteúdo |
|---|---|
| Requisitos | RF-06, RF-08 |
| Objetivo | Verificar a preservação e a correção do horário e a recuperação completa dos registros pendentes após indisponibilidade da central |
| Material | Nó e central em bancada; acesso para regravar o DS1302 via comando de manutenção; firmware da central com opção de enviar um horário arbitrário |

**Procedimento**

1. Com o nó sincronizado, reiniciar o ESP pelo botão e conferir o horário; repetir removendo a 18650 por 1 min com a CR2032 instalada.
2. Regravar o DS1302 com +60 s e aguardar a próxima sessão.
3. Configurar a central para enviar um horário absurdo (ano 2000) numa sessão e verificar a rejeição.
4. Em modo de produção, desligar a central por 12 h (72 registros pendentes) e religá-la.
5. Acompanhar as sessões seguintes até `backlog = 0` no STATUS e consultar o banco.

**Resultado esperado**

- Horário preservado (±2 s) após reset e após remoção da 18650.
- Desvio de 60 s corrigido na sessão seguinte para |Δ| ≤ 2 s, com `clock_adjusted`.
- Horário absurdo rejeitado, sem alteração do DS1302.
- Os 72 registros chegam em até 2 sessões; nenhuma lacuna de `seq` e nenhuma duplicata.

| Resultado obtido | Veredito | Evidência |
|---|---|---|
| — | A executar | `ensaios/ct04/` |

#### CT-05 — Medição da bateria e política de energia

| Campo | Conteúdo |
|---|---|
| Requisitos | RF-12 |
| Objetivo | Calibrar a medição da tensão e verificar as transições de nível com histerese |
| Material | Fonte de bancada ajustável no lugar da 18650 (terminais B+/B− do BMS); multímetro; nó em modo de teste com o módulo de tensão ligado ao SYS OUT do CN3065 |

**Procedimento**

1. **P-VBAT:** conferir os resistores do módulo; ajustar a fonte para 3,0; 3,4; 3,7; 4,0 e 4,2 V e comparar a leitura do nó com o multímetro nos terminais da fonte; medir também a diferença entre BATT IN e SYS OUT em repouso e durante uma transmissão; gravar `K_CAL` na NVS e repetir.
2. Partindo de 4,0 V, reduzir a tensão em passos de 50 mV, mantendo cada passo por 4 ciclos, até 3,20 V.
3. Subir de volta em passos de 50 mV até 3,70 V.
4. Conferir nos registros o campo `power_level`, a flag `vib_skipped` e a cadência de LoRa em cada nível.

**Resultado esperado**

- Erro ≤ 50 mV em toda a faixa, após calibração.
- Diferença entre BATT IN e SYS OUT registrada; se for maior que ~20 mV em repouso, ela é compensada no firmware ou o ponto de medição é revisto.
- Cada limiar de entrada da seção 8.3 muda o nível após 3 leituras consecutivas; na subida, a mudança só ocorre nos limiares de saída.
- Comportamento de cada nível conforme a tabela da seção 8.3.

| Resultado obtido | Veredito | Evidência |
|---|---|---|
| — | A executar | `ensaios/ct05/` |

#### CT-06 — Caracterização energética do nó

| Campo | Conteúdo |
|---|---|
| Requisitos | RNF-05, RNF-06 |
| Objetivo | Substituir os proxies do orçamento energético por medições do nó completo |
| Material | Nó completo com BMS instalado; amperímetro em série com a 18650 (multímetro na escala de mA para o deep sleep; INA219 ou shunt com osciloscópio para os estados curtos); calculadora `beecooler_budget.py` |

**Procedimento**

1. **P4c:** com o nó em deep sleep de produção, medir a corrente no lado da 18650 por pelo menos 1 min. Repetir com o módulo de tensão desconectado e registrar as duas leituras; a diferença deve ser ~0,1 mA.
2. **P4d:** medir a corrente média em cada estado: boot, leitura dos SHT30, janela de vibração com light sleep, processamento.
3. **P28:** medir a corrente do DX-LR32 em TX, RX e sleep a 3,3 V e o tempo de saída do sleep até o AUX indicar pronto.
4. **P-SD1/2/3:** medir corrente e tempo de montagem, escrita de 96 kB, `fsync` e desmontagem do SD em 100 ciclos, registrando o pior caso.
5. Atualizar a calculadora com os valores medidos e recalcular o consumo diário da Fase 1.

**Resultado esperado**

- Deep sleep ≤ 4 mA. Acima disso, o procedimento é diagnosticar qual bloco não dormiu (P4a/P4b), e não redimensionar a energia.
- Consumo diário calculado ≤ 100 mAh/dia.

| Resultado obtido | Veredito | Evidência |
|---|---|---|
| — | A executar | `ensaios/ct06/`, calculadora atualizada |

#### CT-07 — Campanha de coleta com a tiúba

| Campo | Conteúdo |
|---|---|
| Requisitos | RF-10, RF-11, RNF-01, RNF-02, RNF-03, RNF-04, RNF-07, RNF-08, RNF-09 |
| Objetivo | Verificar a operação contínua do sistema completo nas condições reais da campanha e produzir o dataset da Fase 1 |
| Material | Nó sensor completo instalado na colmeia; central e coletor ligados na casa; diário de campo; trena para registrar a distância nó–central; câmera para registrar a montagem |

**Procedimento**

1. Registrar com foto a montagem do ADXL345 e a posição da sonda interna; medir a distância nó–central.
2. Antes de instalar a colônia (ou com a caixa vazia equivalente), coletar algumas horas de referência de caixa vazia (P6).
3. Instalar o nó com a bateria carregada, anotar a tensão inicial e o horário de início no diário.
4. Operar por no mínimo 7 dias, sem recarga manual, anotando no diário toda intervenção ou evento externo.
5. Acompanhar o dashboard diariamente, registrando capturas e eventuais alertas de nó silencioso.
6. Ao fim, extrair o SD, exportar o banco e rodar o script de análise, que calcula: slots esperados × registros no ring; registros no ring × registros no banco; frames confirmados na primeira tentativa; tempo acordado por ciclo (p95); \|Δ\| de relógio por sessão; janelas e registros com CRC válido; menor nível de energia atingido; número de reinicializações não planejadas.

**Resultado esperado**

- ≥ 98% dos slots com registro no ring (descontado o diário) e ≥ 99% dos registros do ring no banco.
- ≥ 90% dos frames confirmados na primeira tentativa.
- 7 dias ou mais sem recarga, sem reinicialização não planejada e sem nível Crítico.
- Tempo acordado p95 ≤ 15 s (≤ 20 s com LoRa); \|Δ\| ≤ 5 s em ≥ 99% das sessões; ≥ 99,9% de CRC válido.
- Dashboard com as séries, a tensão e o estado do enlace durante toda a campanha.

| Resultado obtido | Veredito | Evidência |
|---|---|---|
| — | A executar | `ensaios/ct07/`, dataset da campanha, diário de campo |

#### CT-08 — Atuador em bancada

| Campo | Conteúdo |
|---|---|
| Requisitos | RF-15, RF-16 |
| Objetivo | Verificar o acionamento temporizado da bomba por comando ESP-NOW e os bloqueios de segurança |
| Material | Atuador completo (C3, SLA, driver, bomba, circuito hidráulico com bypass e bicos); emissor de teste por ESP-NOW (ESP32 avulso ou o nó S3 em modo manutenção); cronômetro ou vídeo; fonte ajustável para simular a SLA |

**Procedimento**

1. Enviar comandos de 30, 60 e 120 s, três vezes cada, cronometrando o tempo real de bomba ligada.
2. Enviar um comando acima da duração máxima absoluta e verificar o corte.
3. Reiniciar o C3 durante um acionamento e verificar o estado da bomba após o boot.
4. Com a chave-boia em posição de vazio, enviar um comando.
5. Com a SLA simulada abaixo do limiar, enviar um comando.
6. Conferir o log de acionamentos do C3 e as confirmações recebidas pelo emissor.

**Resultado esperado**

- Erro ≤ 1 s em todas as durações; corte na duração máxima absoluta.
- Bomba desligada após reset do C3.
- Comandos recusados com motivo correto nos casos de boia vazia e SLA baixa.
- Todos os acionamentos registrados com horário, duração e motivo; todas as confirmações recebidas.

| Resultado obtido | Veredito | Evidência |
|---|---|---|
| — | A executar | `ensaios/ct08/`, vídeo |

#### CT-09 e CT-10 — Fase 2

Serão detalhados no próximo ciclo. Em linhas gerais:

- **CT-09** verifica a decisão na borda (RF-17) forçando T_in acima do limiar em bancada e conferindo o envio do comando, o respeito às regras de segurança e o EVENT imediato na central (RF-19).
- **CT-10** repete CT-01 a CT-04 sem o microSD, com o mesmo binário (RF-18).

### 14.4 Testes de robustez

Além dos casos de teste, as situações abaixo seguem a lógica da seção 5.2 da AP1.

| Situação | Como provocar | Comportamento esperado | Onde entra |
|---|---|---|---|
| a) Sensor ausente na inicialização | ligar o nó sem um dos SHT30 ou sem o ADXL345 | o nó **inicia e coleta** o que estiver disponível, com sentinela e flag para o ausente | CT-01 |
| b) Sensor perdido em operação | desconectar com o nó rodando | sentinela + flag no ciclo seguinte; recuperação sem reboot | CT-01 |
| c) Reinicialização do nó durante a sessão LoRa | cortar a energia no meio da sessão | nenhum registro perdido; frames não confirmados reenviados na sessão seguinte | CT-02 |
| d) Reinicialização da central | reiniciar o WROOM durante a recepção | nenhum registro perdido; `acked_up_to` reconstruído a partir do que foi persistido | CT-03 |
| e) Remoção do microSD | retirar o cartão com o nó rodando | operação continua; `sd_fail`; nenhum reboot em laço | CT-02 |
| f) Perda e restabelecimento do enlace | desligar a central por horas | backlog recuperado integralmente quando ela volta | CT-04 |
| g) Perda da CR2032 | remover a CR2032 e a 18650 juntas | boot com `rtc_invalid`; `seq` recuperado pela varredura da flash; horário corrigido no primeiro ACK | CT-02/CT-04 |
| h) Frame corrompido | corrupção proposital na central | nenhum registro nem ACK para o frame corrompido | CT-03 |
| i) Horário absurdo da central | enviar ano 2000 no ACK | horário rejeitado; DS1302 inalterado | CT-04 |

O item (a) **muda de propósito** em relação à Fase 0, em que o nó bloqueava a coleta se um sensor faltasse na inicialização. Numa campanha de vários dias sem supervisão contínua, perder todos os dados por causa de um único sensor é pior do que coletar parcialmente com a falha sinalizada (D52).

---

## 15. Rastreabilidade

### 15.1 Requisitos, onde são atendidos e como são verificados

| Requisito | Onde é atendido (este documento) | Caso de teste | Situação |
|---|---|---|---|
| RF-01 | 4.1, 4.2, 4.3, 5.2 | CT-01 | A verificar |
| RF-02 | 4.4, 5.8 | CT-01 | A verificar |
| RF-03 | 4.5 | CT-01 | A verificar |
| RF-04 | 5.6 | CT-02 | A verificar |
| RF-05 | 5.7, 5.8, 5.9, 9.1 | CT-02 | A verificar |
| RF-06 | 3.3 (UC-06), 6.4 | CT-04 | A verificar |
| RF-07 | 6.5, 6.6 | CT-03 | A verificar |
| RF-08 | 3.3 (UC-07), 6.5 | CT-04 | A verificar |
| RF-09 | 6.7, 7.1 | CT-03 | A verificar |
| RF-10 | 7.3 | CT-07 | A verificar |
| RF-11 | 6.3 (STATUS), 7.2, 7.3 | CT-07 | A verificar |
| RF-12 | 8.2, 8.3 | CT-05 | A verificar |
| RF-13 | 3.3 (UC-10), 5.5, 5.6 | CT-02 | A verificar |
| RF-14 | 4.3, 4.4, 5.3 | CT-01 | A verificar |
| RF-15 | Briefing v4.1, seção 8 | CT-08 | A verificar |
| RF-16 | Briefing v4.1, seção 8.4 | CT-08 | A verificar |
| RF-17 | 9.2 | CT-09 | Fase 2 |
| RF-18 | 9.1 | CT-10 | Fase 2 |
| RF-19 | 6.3 (EVENT), 9.2 | CT-09 | Fase 2 |
| RNF-01 | 4.1, 5.6 | CT-07 | A verificar |
| RNF-02 | 3.3 (UC-07), 6, 7 | CT-07 | A verificar |
| RNF-03 | 6.6 | CT-07 | A verificar |
| RNF-04 | 8 | CT-07 | A verificar |
| RNF-05 | 8.4 | CT-06 | A verificar |
| RNF-06 | 8.4 | CT-06 | A verificar |
| RNF-07 | 4.2 | CT-07 | A verificar |
| RNF-08 | 3.3 (UC-06) | CT-07 | A verificar |
| RNF-09 | 5.6, 5.8, 5.9 | CT-02, CT-07 | A verificar |
| RNF-10 | Briefing v4.1, seção 10.1 | Lista de materiais | A verificar |
| RNF-11 | 6.3 (HELLO/STATUS) | Verificação de versão | A verificar |

### 15.2 Correspondência com os requisitos da AP1 (Ciclo 1 / Fase 0)

| Requisito da AP1 | Resultado na AP1 | Neste ciclo | O que mudou |
|---|---|---|---|
| RF-01 — T/RH a cada 500 ms | Atendido | RF-01 | Cadência de 10 min alinhada ao calendário, mediana de 3 leituras |
| RF-02 — aceleração no mesmo registro | Atendido | RF-02, RF-03 | Janela contínua de 10 s a 1600 Hz via FIFO, no lugar de uma leitura a 2 Hz; features espectrais |
| RF-03 — ESP-NOW LR com até 5 tentativas | Atendido | RF-07 | LoRa; 4 tentativas; abort da sessão em falha |
| RF-04 — CRC32, duplicatas, remontagem | Atendido | RF-09 | Sem fragmentação; ACK só após persistência |
| RF-05 — registro no host | Atendido | RF-10 | SQLite com chave única no lugar dos CSVs |
| RF-06 — dashboard a cada 2 s | Atendido | RF-11 | Inclui bateria, backlog, RSSI e alerta de nó silencioso |
| RNF-01 — ≥ 95% dos batches a ~300 m | **Não atendido** | adiado; RNF-02 e RNF-03 neste ciclo | O alcance no apiário sai deste ciclo (seção 13.3); aqui, entrega final com backlog e qualidade do enlace a curta distância |
| RNF-02 — ≥ 1 h com alimentação própria | Atendido | RNF-04 | ≥ 7 dias com energia solar, sem recarga |
| RNF-03 — custo ≤ R$ 300 | Atendido | RNF-10 | Novo limite para o nó completo com energia, rádio dedicado e RTC |

Limitações da AP1 (seção 7.1) tratadas por estes requisitos:

| Limitação registrada na AP1 | Requisito que a trata |
|---|---|
| Dados não entregues eram perdidos (sem retenção local) | RF-04, RF-08 |
| Sem relógio de calendário | RF-06 |
| ADXL345 lido a 2 Hz, sem FIFO, sem análise espectral | RF-02, RF-03 |
| Driver não revalidava o sensor em operação | RF-14 |
| Sem RSSI e sem registro de tentativas | RF-11 (RSSI/SNR no ACK e no STATUS); campo `attempt` no frame (RNF-03) |
| Orçamento energético apenas projetado | RNF-05, RNF-06 (CT-06) |
| Versão gravada não identificável pelo repositório (auditoria) | RNF-11 |


---

## 16. Referências

1. Rigakis, I.; Potamitis, I.; Tatlas, N.-A.; Psirofonia, G.; Tzagaraki, E.; Alissandrakis, E. *A Low-Cost, Low-Power, Multisensory Device and Multivariable Time Series Prediction for Beehive Health Monitoring.* Sensors 2023, 23, 1407. https://doi.org/10.3390/s23031407
2. Książek, P.; Szlachetko, B.; Roman, A. *Honey Bee Lifecycle Activity Prediction Using Non-Invasive Vibration Monitoring.* Appl. Sci. 2026, 16, 188. https://doi.org/10.3390/app16010188
3. BeeCooler — Briefing de Decisões v4.1 (14/09/2026).
4. BeeCooler — Revisão de Energia, Telemetria e TinyML v6.0 (01/10/2026).
5. BeeCooler — Auditoria Técnica da Fase 0 (AP1).
6. BeeCooler — AP1: Relatório do Ciclo de Projeto 1 (Fase 0), 21/09/2026 — formato dos requisitos (Cap. 2), dos casos de teste (Cap. 5 e Apêndice C) e limitações (Cap. 7).
7. DX-SMART — DX-LR32-900T22D Technical Manual v2.0 e UART Application Guide v2.0.
8. Analog Devices — ADXL345 datasheet.
9. Sensirion — SHT3x datasheet.
10. Espressif — ESP32-S3 Technical Reference Manual e documentação do ESP-IDF (deep/light sleep, ADC, partições).
