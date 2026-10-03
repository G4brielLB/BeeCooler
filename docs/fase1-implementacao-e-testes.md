# BeeCooler — Fase 1: o que foi implementado e como montar para testar

**Origem:** commit `9659f61` (branch `dev`), construído sobre `docs/BeeCooler_arquitetura_software_v1_2.md` ([A]) e `docs/BeeCooler — Integração em Nuvem (AWS Serverless) v1.1.md` ([N]).
**Status:** 🟡 firmware compilável e com testes de lógica no host, **ainda não validado em hardware**. Pinagem, registradores do ADXL345 e timings precisam de conferência em bancada (ver seção 6).
**Fora do escopo:** o atuador (ESP32-C3, Ramo 2 de `docs/hardware-fases.md`, RF-15/16) não tem firmware. As pastas `src/sensor_bench`, `src/sensor_espnow_test` e `src/gateway_espnow_test` são da Fase 0 e não foram alteradas.

---

## 1. Resumo

A Fase 1 entrega o caminho completo do dado:

```text
 SHT30 x2 + ADXL345 + bateria
        │  (nó sensor, ESP32-S3, deep sleep entre ciclos)
        ▼
 ring em flash (beelog) ──► sessão LoRa (DX-LR32) ──► central (ESP32-WROOM)
        │                                                   │
   microSD (bruto)                              fila LittleFS ──► Wi-Fi/HTTPS ──► AWS
```

- O nó mede a cada ciclo, grava o registro de 24 B no ring e, periodicamente, envia os registros pendentes por LoRa, só marcando como confirmados após o ACK.
- A central só dá ACK **depois** de persistir no LittleFS; a nuvem recebe em lote por HTTPS com token Bearer.

---

## 2. O que foi implementado

### 2.1 Bibliotecas (`lib/`)

| Lib | Função |
|---|---|
| `BeeCoolerRecord` | `RecordV1` (24 B), `RingSlot` (32 B), cabeçalho do dado bruto, CRC-8/CRC-16, codificação de bandas em dB, flags e nível de energia |
| `BeeCoolerLink` | Protocolo LoRa v2: quadros DATA, STATUS, HELLO e ACK, com encode/decode e validação |
| `BeeCoolerLora` | Driver do DX-LR32 (modo transparente): UART, pinos M0/M1, AUX como fim de TX, modos normal/sleep |
| `BeeCoolerLogic` | Agenda (slot mais próximo), conversão epoch↔data, histerese de nível de energia, parâmetros por nível |
| `BeeCoolerVib` | Processamento da janela do acelerômetro (RMS, pico, bandas) |

`BeeCoolerProtocol` e `BeeCoolerSensors` são da Fase 0.

### 2.2 Nó sensor (`src/sensor/`, ESP32-S3 N16R8)

| Módulo | Função |
|---|---|
| `main.cpp` | Ciclo: acorda, mede, grava no ring, abre sessão LoRa quando é a hora, volta a dormir. No cold boot, envia HELLO e abre o console de manutenção |
| `Sht30Pair` | Dois SHT30 (0x44) em dois barramentos I²C; 3 leituras por sensor |
| `Adxl345Fifo` | Janela de 16000 amostras por eixo (1600 Hz, ±2 g) lida pelo FIFO com watermark no INT1 |
| `Ds1302Clock` | Relógio de 3 fios; corrigido pelo ACK do HELLO quando inválido ou desviado |
| `BatteryMonitor` | Leitura da bateria no GPIO4 com divisor (×5,0) e calibração (`kcal`, `vdiv`) |
| `FlashRing` | Ring na partição `beelog` (8 MB, 262.144 slots de 32 B) |
| `RawSink` | Dado bruto no microSD; o mesmo binário opera sem cartão (`-DBEE_NO_SD` força isso) |
| `LoraSession` | HELLO/DATA/STATUS com ACK, backoff aleatório 50–250 ms, até 36 quadros por sessão |
| `Console` | Console serial de manutenção (seção 5.4) |
| `NodeState` | Estado persistente entre sleeps (último seq, confirmado, nível de energia, falhas) |

Tempos padrão: ciclo de **10 min**, LoRa a cada **1 h**. Em deep sleep, M0/M1 do módulo ficam presos em nível alto (`gpio_hold`).

### 2.3 Central (`src/gateway/`, ESP32-WROOM)

| Módulo | Função |
|---|---|
| `RadioTask` (core 1) | Recebe LoRa, valida CRC, deduplica, persiste no LittleFS e só então envia ACK |
| `GatewayStore` | Fila de uplink (itens de 52 B), quarentena de lotes rejeitados e estado por nó (até 8 nós) |
| `UplinkTask` (core 0) | Wi-Fi + NTP; `POST /telemetry` em lotes (até 128 itens / 32 KB) e `POST /gateway/heartbeat` a cada 15 min; backoff 5 s → 15 s → 60 s → 5 min |
| `CloudConfig` | Wi-Fi, URL, ID e token guardados na NVS (nunca no repositório) |
| `AmazonRootCA.h` | CA raiz embutida para validar o TLS |
| `main.cpp` | Console serial (seção 5.3) |

### 2.4 Build, partições e testes

- `partitions/sensor_16mb.csv` (app 4 MB + `beelog` 8 MB) e `partitions/gateway_4mb.csv` (app 2 MB + LittleFS 1 MB).
- Ambientes PlatformIO:

| Env | Uso |
|---|---|
| `sensor` | Firmware do nó, tempos reais |
| `sensor_fast` | Modo de teste acelerado (D53): ciclo de 30 s, LoRa a cada 3 min |
| `gateway` | Firmware da central |
| `gateway_fault` | Central com injeção de falhas (CT-03 / CT-12) |
| `native` | Testes unitários no PC |

- Testes no host: `test/test_record_link` (layouts, CRC, codificação, ida e volta dos quadros) e `test/test_logic_vib` (agenda, energia, vibração).
- `docs/pinos.md` (guia de ligação) e `docs/hardware-fases.md` (lista de materiais).

---

## 3. Material necessário

**Nó sensor:** ESP32-S3 N16R8, 2× SHT30 à prova d'água, ADXL345 GY-291, DS1302 com CR2032, módulo microSD (3,3 V) com cartão, DX-LR32 (868/915 MHz) com antena, módulo de tensão 0–25 V, bateria 18650 + CN3065 + TPS63020 (3,3 V), 2× resistores de 100 kΩ, capacitor de 100–470 µF.

**Central:** ESP32-WROOM DevKit (não WROVER) + DX-LR32 com antena, cabo USB.

**Bancada:** PC com PlatformIO, adaptador USB-TTL do kit LR32, multímetro, rede Wi-Fi 2,4 GHz e, para o teste de nuvem, URL da API, ID do gateway e token.

---

## 4. Montagem

Fonte da verdade: `src/sensor/pins.h` e `src/gateway/pins.h`. O detalhe completo (diagramas, motivos, alimentação) está em [`pinos.md`](pinos.md). Pinagem 🟡 **proposta**.

### 4.1 Nó sensor

| Periférico | Sinal | GPIO | Pino do módulo |
|---|---|---:|---|
| Bateria (módulo 0–25 V) | Leitura (ADC1) | 4 | `S` |
| CN3065 (opcional) | CHRG | 15 | `CHRG` |
| SHT30 interno | SDA | 8 | `SDA` |
| SHT30 interno | SCL | 9 | `SCL` |
| SHT30 externo | SDA | 17 | `SDA` |
| SHT30 externo | SCL | 18 | `SCL` |
| SPI (ADXL345 + microSD) | SCK | 12 | ADXL `SCL` / SD `SCK` |
| SPI (ADXL345 + microSD) | MOSI | 11 | ADXL `SDA` / SD `MOSI` |
| SPI (ADXL345 + microSD) | MISO | 13 | ADXL `SDO` / SD `MISO` |
| ADXL345 | CS | 10 | `CS` |
| ADXL345 | INT1 | 16 | `INT1` |
| ADXL345 | INT2 | 7 | `INT2` |
| microSD | CS | 14 | `CS` |
| DX-LR32 | ESP TX → módulo | 38 | `RXD` |
| DX-LR32 | ESP RX ← módulo | 39 | `TXD` |
| DX-LR32 | M0 | 1 | `M0` |
| DX-LR32 | M1 | 2 | `M1` |
| DX-LR32 | AUX | 5 | `AUX` |
| DS1302 | CE / RST | 47 | `RST` |
| DS1302 | I/O / DATA | 40 | `DAT` |
| DS1302 | SCLK | 21 | `CLK` |

### 4.2 Central

| Periférico | Sinal | GPIO | Pino do módulo |
|---|---|---:|---|
| DX-LR32 | ESP RX2 ← módulo | 16 | `TXD` |
| DX-LR32 | ESP TX2 → módulo | 17 | `RXD` |
| DX-LR32 | M0 | 19 | `M0` |
| DX-LR32 | M1 | 21 | `M1` |
| DX-LR32 | AUX | 18 | `AUX` |

### 4.3 Cuidados principais

1. TX/RX do DX-LR32 **cruzados** nos dois lados.
2. Tudo em 3,3 V, GND comum.
3. Pull-up de 100 kΩ para 3V3 nos CS do ADXL345 (GPIO10) e do microSD (GPIO14).
4. Capacitor de 100–470 µF perto do DX-LR32.
5. O módulo microSD precisa soltar o MISO com CS em alto (ele divide o barramento com o ADXL345).
6. Meça os resistores do módulo de tensão antes de ligar o pino `S` (VCC→S ≈ 30 kΩ, S→GND ≈ 7,5 kΩ); se diferirem, ajuste com `vdiv`.
7. Na bancada, LEDs do DevKit elevam a corrente de deep sleep (RNF-05 ≤ 4 mA).

### 4.4 Configuração prévia dos dois DX-LR32 🟡

O driver **não configura** o módulo, só usa o modo transparente. Antes de ligar ao ESP, use o adaptador USB-TTL do kit para deixar os dois módulos com o **mesmo canal, mesma taxa no ar e UART em 115200**. Confira os valores no manual do DX-LR32.

### 4.5 Checklist antes de ligar

- [ ] Continuidade entre todos os GND.
- [ ] 3,3 V estável no TPS63020 antes de ligar o ESP.
- [ ] M0/M1 em 1 e 2 (nó) e 19 e 21 (central).
- [ ] Pull-ups de CS e CR2032 no DS1302.
- [ ] Antenas conectadas aos dois DX-LR32 antes de transmitir.

---

## 5. Como testar

### 5.1 Sem hardware

```bash
pio test -e native            # testes unitários no PC
pio run -e sensor -e gateway  # compila; os static_assert de pins.h validam a pinagem
```

### 5.2 Gravar

```bash
pio run -e gateway -t upload        # central
pio run -e sensor_fast -t upload    # nó, modo acelerado
pio device monitor -e gateway       # 115200
```

Use `sensor_fast` para os primeiros testes. Com o USB-CDC do S3, escolha a porta serial correta no `upload`/`monitor` (veja `UPLOAD_MONITOR_ESP32.md`).

### 5.3 Configurar e conferir a central (console serial)

```text
set wifi <ssid> <senha>       (SSID sem espaços)
set url https://<id>.execute-api.<regiao>.amazonaws.com/v1
set gw GW01
set token <token>
show                          estado, contadores de rádio e de uplink (sem o token)
queue                         pendentes / em quarentena
reboot
```

Sem Wi-Fi/URL/token a central continua recebendo e guardando no LittleFS; o envio acontece quando a configuração fica completa.

### 5.4 Preparar o nó (console de manutenção)

Logo após o **cold boot** (reset ou energização) o nó espera **3 s**; envie qualquer caractere pela serial para entrar:

```text
status            seq, confirmado, nível de energia, falhas, calibração
time              hora do DS1302
settime <epoch>   ajusta o relógio (ex.: o resultado de `date +%s`)
vbat              tensão lida
kcal <mV>         calibra com a tensão medida no multímetro (2000–5000 mV)
vdiv <valor>      divisor do módulo de tensão
dump [n]          últimos n registros do ring
exit              segue para o ciclo normal
```

Ordem sugerida: `settime`, `time`, `vbat`, `kcal`, `exit`.

### 5.5 Teste ponta a ponta (modo rápido)

1. Central ligada e com o monitor aberto.
2. Nó em `sensor_fast`: no serial aparecem `[boot] cold boot ...`, `[hello] ACK`, `[power] ...`, `[rec] seq=...`, `[sleep] N s`.
3. A cada ciclo de 30 s sai um registro; a cada 3 min o nó abre sessão LoRa.
4. Na central, `show` deve mostrar `frames`, `stored` e `acks` crescendo, `crc=0` e `gaps=0`.
5. Com a nuvem configurada, `uplink: wifi=1 ntp=1 ... ok=N http=200` e `remote_stored` crescendo; `queue` volta a 0 pendentes.
6. No nó, `dump 5` mostra os registros e `status` mostra `confirmed` alcançando `last_seq`.

Critério de aprovação: nenhum registro perdido nem duplicado entre `dump` (nó) e o que a nuvem armazenou.

### 5.6 Testes de falha (`gateway_fault`)

```bash
pio run -e gateway_fault -t upload
```

| Comando | Efeito | O que observar |
|---|---|---|
| `fault corrupt <n>` | Corrompe um quadro a cada n | `crc` sobe, o nó retransmite, sem perda |
| `fault dropack <n>` | Descarta n ACKs | Nó reenvia, `dup` sobe, sem duplicata na fila |
| `fault reboot 1` | Reinicia após receber, antes do ACK | Dado persistido, nó reenvia, deduplicado |
| `fault drop200 <n>` | Ignora n respostas 200 da nuvem | Lote reenviado, `duplicates_remote` sobe, sem duplicata |

### 5.7 Tempos reais

Regravar o nó com `pio run -e sensor -t upload`: ciclo de 10 min, LoRa a cada 1 h. Use para o teste longo (bateria, deep sleep, deriva do DS1302).

---

## 6. Pendências e itens não validados em hardware

- 🟡 **Pinagem do S3 e da central** (proposta; M0/M1 em GPIO 1/2 por exigência de hold em deep sleep).
- 🟡 Registradores e taxas do ADXL345 (ODR 1600 Hz, FIFO, watermark).
- 🟡 RSSI/SNR no ACK: a central envia 0 (desconhecido).
- 🟡 `kAckTimeoutMs = 400` é valor inicial; fixar em p99 + 30% após medir o RTT.
- 🟡 CA da Amazon embutida: verificada por SHA-256, falta testar o TLS real.
- 🟡 Corrente de deep sleep (RNF-05) com a placa final.
- Detecção de atividade do ADXL345 (D51) desligada por padrão (`kMechEventEnabled = false`).
- Atuador ESP32-C3 e ESP-NOW (RF-15/16): próximo passo natural.
