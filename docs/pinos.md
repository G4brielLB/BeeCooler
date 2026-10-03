# BeeCooler — Ligação física dos pinos (Fase 1)

**Fonte da verdade:** `src/sensor/pins.h` (nó sensor, ESP32-S3) e `src/gateway/pins.h` (central, ESP32-WROOM). Este documento é a versão de bancada desses arquivos: se um pino mudar, mude os três.
**Status:** 🟡 **proposta**. Os docs de arquitetura não definem a pinagem, apenas VBAT no GPIO4, INT1 no GPIO16 e INT2 no GPIO7 do ADXL345. O restante foi escolhido para respeitar as restrições do S3 e precisa ser conferido na bancada.
**Fora deste documento:** o atuador (ESP32-C3, Ramo 2) ainda não tem firmware, portanto não tem pinagem aqui.

---

## 1. Nó sensor — ESP32-S3-DevKitC-1 (N16R8)

### 1.1 Tabela de ligação

Os nomes da coluna "Pino do módulo" são os comuns de cada placa. **Confira a serigrafia da sua**, porque há variantes.

| Periférico | Sinal | GPIO do ESP32-S3 | Constante | Pino do módulo | Observação |
|---|---|---:|---|---|---|
| Bateria (módulo 0–25 V) | Leitura (ADC1) | **4** | `kPinVbat` | `S` | O pino `+` do módulo **não é usado**. `−` vai ao GND. Terminais de potência no SYS OUT do CN3065 (v1.2, 8.2) |
| CN3065 (opcional) | CHRG | **15** | `kPinCharging` | CHRG | Coletor aberto, nível baixo = carregando. Pull-up interno. Pode ficar sem ligar (`-1` desliga) |
| SHT30 interno | SDA | **8** | `kPinShtIntSda` | SDA | I²C do controlador 0 |
| SHT30 interno | SCL | **9** | `kPinShtIntScl` | SCL | |
| SHT30 externo | SDA | **17** | `kPinShtExtSda` | SDA | I²C do controlador 1. Barramento separado porque os dois sensores usam o endereço 0x44 |
| SHT30 externo | SCL | **18** | `kPinShtExtScl` | SCL | |
| SPI (ADXL345 + microSD) | SCK | **12** | `kPinSpiSck` | ADXL `SCL` / SD `SCK` | Barramento compartilhado |
| SPI (ADXL345 + microSD) | MOSI | **11** | `kPinSpiMosi` | ADXL `SDA` / SD `MOSI` | |
| SPI (ADXL345 + microSD) | MISO | **13** | `kPinSpiMiso` | ADXL `SDO` / SD `MISO` | |
| ADXL345 (GY-291) | CS | **10** | `kPinAdxlCs` | `CS` | Pull-up externo de 100 kΩ para 3V3 |
| ADXL345 (GY-291) | INT1 | **16** | `kPinAdxlInt1` | `INT1` | Watermark do FIFO; acorda o light sleep |
| ADXL345 (GY-291) | INT2 | **7** | `kPinAdxlInt2` | `INT2` | Detecção de atividade (D51, desligada por padrão) |
| microSD | CS | **14** | `kPinSdCs` | `CS` | Pull-up externo de 100 kΩ para 3V3 |
| DX-LR32 | ESP TX → módulo | **38** | `kPinLoraTx` | `RXD` | UART1, 115200 baud |
| DX-LR32 | ESP RX ← módulo | **39** | `kPinLoraRx` | `TXD` | TX e RX são **cruzados** |
| DX-LR32 | M0 | **1** | `kPinLoraM0` | `M0` | Obrigatoriamente GPIO RTC (0–21), veja 1.3 |
| DX-LR32 | M1 | **2** | `kPinLoraM1` | `M1` | Idem |
| DX-LR32 | AUX | **5** | `kPinLoraAux` | `AUX` | Alto = ocupado; a queda marca o fim do TX |
| DS1302 | CE / RST | **47** | `kPinRtcCe` | `RST` | |
| DS1302 | I/O / DATA | **48** | `kPinRtcIo` | `DAT` | |
| DS1302 | SCLK | **21** | `kPinRtcSclk` | `CLK` | |

GPIO usados no total: **1, 2, 4, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 21, 38, 39, 47, 48** (21 sinais, nenhum repetido).

### 1.2 Diagrama

```text
                           ESP32-S3 (N16R8)
   Bateria 0–25 V   S ───► GPIO4
   CN3065 CHRG (opc.) ───► GPIO15

   SHT30 interno   SDA ◄─► GPIO8      SCL ◄── GPIO9
   SHT30 externo   SDA ◄─► GPIO17     SCL ◄── GPIO18

   SPI compartilhado
        SCK  GPIO12 ─────┬──► ADXL345 SCL    ┬──► microSD SCK
        MOSI GPIO11 ─────┼──► ADXL345 SDA    ┼──► microSD MOSI
        MISO GPIO13 ◄────┴─── ADXL345 SDO    └─── microSD MISO
        CS   GPIO10 ──────► ADXL345 CS   (pull-up 100 kΩ → 3V3)
        CS   GPIO14 ──────► microSD CS   (pull-up 100 kΩ → 3V3)
   ADXL345 INT1 ───► GPIO16          ADXL345 INT2 ───► GPIO7

   DX-LR32   GPIO38 ──► RXD      GPIO39 ◄── TXD      (cruzado)
             GPIO1  ──► M0       GPIO2  ──► M1       GPIO5 ◄── AUX

   DS1302    GPIO47 ──► RST      GPIO48 ◄─► DAT      GPIO21 ──► CLK
```

### 1.3 Por que estes GPIO (e quais evitar)

| Restrição | GPIO | Efeito |
|---|---|---|
| Flash | 26 a 32 | Não usar |
| PSRAM octal (N16R8) | 33 a 37 | Não usar |
| USB nativo | 19, 20 | Livres para o USB-CDC do `Serial` |
| Strapping | 0, 3, 45, 46 | Evitados, definem o modo de boot |
| UART0 do DevKit | 43, 44 | Reservados ao conversor USB-serial |
| **Hold em deep sleep** | só **0 a 21** | M0/M1 do DX-LR32 precisam manter nível alto enquanto o ESP dorme (`gpio_hold_en`), senão o módulo sai do modo sleep e gasta corrente de RX/TX. Por isso ficam em **1 e 2** |

O `pins.h` confere isso na compilação: `static_assert` para M0/M1 ≤ 21, pinos distintos e nenhum pino reservado.

---

## 2. Central — ESP32-WROOM DevKit + DX-LR32

| Sinal | GPIO do ESP32 | Constante | Pino do módulo | Observação |
|---|---:|---|---|---|
| ESP RX ← módulo | **16** | `kPinLoraRx` | `TXD` | UART2, 115200 baud (igual ao nó) |
| ESP TX → módulo | **17** | `kPinLoraTx` | `RXD` | TX e RX **cruzados** |
| M0 | **25** | `kPinLoraM0` | `M0` | A central mantém o módulo sempre em modo normal (recebendo) |
| M1 | **26** | `kPinLoraM1` | `M1` | |
| AUX | **27** | `kPinLoraAux` | `AUX` | |

```text
   ESP32-WROOM                    DX-LR32
   GPIO16 (RX2) ◄──────────────── TXD
   GPIO17 (TX2) ───────────────►  RXD
   GPIO25 ─────────────────────►  M0
   GPIO26 ─────────────────────►  M1
   GPIO27 ◄─────────────────────  AUX
   3V3/5V ─────────────────────►  VCC      GND ──── GND
```

- Os GPIO 16/17 só ficam livres no **ESP32-WROOM-32**. No WROVER eles pertencem à PSRAM.
- A central é alimentada por USB (carregador), então o módulo pode usar o 5 V da placa, se o DX-LR32 aceitar. Confira a faixa de VCC no manual do módulo e mantenha os sinais em 3,3 V.

---

## 3. Alimentação e terra (nó sensor)

- **Um único trilho de 3,3 V**, saída do TPS63020, alimenta ESP, ADXL345, SHT30, DS1302, microSD e DX-LR32.
- **GND comum e único**: ESP, módulos, `−` do módulo de tensão, SYS OUT `−` do CN3065 e P− do BMS no mesmo nó elétrico (v1.2, 8.2).
- **Capacitor de 100 a 470 µF** entre 3V3 e GND, junto ao DX-LR32, para o pico de transmissão.
- **Pull-ups de 100 kΩ** para 3V3 nos CS do ADXL345 (GPIO10) e do microSD (GPIO14). Esses GPIO flutuam em deep sleep.
- **Pull-ups do I²C:** as sondas SHT30 à prova d'água costumam trazer resistores; se a leitura falhar, acrescente 4,7 kΩ de SDA e SCL para 3V3 em cada barramento.
- **DS1302:** VCC2 no 3,3 V; CR2032 no VCC1 (bateria de backup). Sem a CR2032 o relógio perde a hora a cada queda de energia.
- **Módulo de tensão 0–25 V:** os terminais de potência (parafusos) vão ao SYS OUT do CN3065. Só o pino `S` liga ao GPIO4.

---

## 4. Cuidados

1. **Conferir a serigrafia.** O M0/M1 do DX-LR32 pode aparecer como MD0/MD1; o `SDA` do GY-291 é o MOSI no modo SPI e o `SDO` é o MISO.
2. **Todos os sinais em 3,3 V.** Nenhum pino do S3 é tolerante a 5 V.
3. **Módulo de microSD:** muitos têm regulador AMS1117 e conversor de nível pensados para 5 V. Com o trilho de 3,3 V do nó, use um módulo que funcione em 3,3 V direto. Como o MISO é compartilhado com o ADXL345, o módulo precisa **liberar o MISO** (tri-state) com o CS em alto, senão o ADXL345 não é lido corretamente.
4. **LEDs do DevKit.** O LED RGB do DevKit-C-1 fica no GPIO48 (v1.0) ou no GPIO38 (v1.1), justamente pinos usados aqui. Os LEDs de bordo e o regulador do DevKit somam à corrente de deep sleep e podem estourar o limite de RNF-05 (≤ 4 mA). Isso vale para a bancada; a placa final não terá esses componentes.
5. **Resistores do módulo de tensão:** antes de ligar o pino `S` ao GPIO4, meça VCC→S (~30 kΩ) e S→GND (~7,5 kΩ) com o módulo desconectado (v1.2, 8.2).
6. **Não existe pino de atuador** nesta tabela. O ESP32-C3 e a bomba serão tratados no firmware do Ramo 2.

---

## 5. Checklist antes de ligar

- [ ] GND comum: continuidade entre todos os GND da tabela da seção 3.
- [ ] 3,3 V estável no trilho do TPS63020 antes de ligar o ESP.
- [ ] TX/RX do DX-LR32 **cruzados** (ESP TX → RXD, ESP RX ← TXD), nos dois lados.
- [ ] M0/M1 no GPIO 1 e 2 (nó) e 25 e 26 (central).
- [ ] Pull-ups de CS no ADXL345 e no microSD; CR2032 no DS1302.
- [ ] Resistores do módulo de tensão conferidos.
- [ ] `pio run -e sensor -e gateway` compila com a pinagem atual.
