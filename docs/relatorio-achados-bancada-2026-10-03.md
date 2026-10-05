# Relatório de achados da bancada — S3, sensores e gateway LoRa

**Data de consolidação:** 03/10/2026, America/Fortaleza.

**Projeto:** BeeCooler.

**Ambientes:** `comunicacao_s3_test` e `comunicacao_wroom_test`.

**Referência local:** branch `dev`, HEAD `b4e2061`, com alterações não commitadas.

Este relatório consolida os logs enviados pelo usuário, as medições e observações físicas relatadas e a inspeção do código durante a sessão. Os últimos logs considerados são os posteriores aos uploads de **8,470 s no gateway** e **10,081 s no S3**, já com M0/M1 como entradas nos ESPs.

O HEAD acima identifica a base do repositório, não uma versão imutável dos binários gravados: existem alterações locais. Os logs completos foram fornecidos na conversa; os trechos abaixo são recortes representativos, não arquivos de captura contínua.

Este documento registra as evidências e pendências encontradas. A especificação local `PROJECT_SPEC.local.md` (arquivo privado, não versionado), datada de 02/09/2026, descreve uma etapa anterior com WROOM/ESP-NOW; este relatório documenta a bancada S3/LoRa observada e não altera decisões de arquitetura nem valida parâmetros científicos finais.

## 1. Estado ao encerrar a coleta de evidências

**A comunicação pelo ar entre o nó e o gateway ainda não foi confirmada.** Os dois módulos respondem a comandos AT, mas o gateway continua com `uart_bytes=0`, e o S3 não recebe ACK.

**Há evidência direta de mau contato no encaixe do S3:** a continuidade de SCL do SHT interno com GPIO9 apareceu ao pressionar a placa; o usuário também relatou pressionar o S3 quando os dois SHT30 voltaram a funcionar.

| Item | Evidência mais recente | Conclusão permitida |
| --- | --- | --- |
| Upload dos dois ESPs | `SUCCESS` nos dois ambientes | Os binários foram gravados; isso não valida periféricos |
| UART do gateway com seu LoRa | `Entry AT`, `OK`, consultas e `Exit AT` | Comunicação local de ida e volta confirmada naquele teste |
| UART do S3 com seu LoRa | Mesma sequência AT completa | Comunicação local de ida e volta confirmada naquele teste |
| Enlace LoRa | S3 tenta enviar; gateway registra zero bytes e zero quadros | Não confirmado |
| SHT30 interno | Leituras válidas nos últimos dois ciclos completos | Funcionou nesses ciclos; histórico de mau contato |
| SHT30 externo | Leituras válidas nos últimos dois ciclos completos | Funcionou nesses ciclos; histórico de ausência de resposta |
| ADXL345 | `DEVID=0xE5`, 16.000 amostras/eixo, zero overruns | Aquisição completa nos últimos ciclos; estabilidade geral pendente |
| RTC DS1302 | Data em 2099, segundos sem avanço normal, outras leituras `FF`/`00` | Data/hora e funcionamento não validados |
| microSD | Últimos ciclos sem montagem; houve teste de escrita/leitura aprovado antes | Funcionamento intermitente, não estabilizado |
| Medição de bateria | Valores variáveis, divisor 5 e ganho 1 | Tensão/calibração não validadas |
| Protoboard do S3 | Funcionamento/continuidade dependentes de pressão | Contato instável confirmado por observação do usuário |

Nenhum resultado isolado de `OK` valida precisão, estabilidade em campo ou autonomia energética.

## 2. Montagem e parâmetros efetivamente utilizados

### 2.1 Equipamentos e alimentação

- Nó sensor: ESP32-S3, alvo PlatformIO `esp32-s3-devkitc-1`.
- Gateway: ESP32-WROOM; upload identificou `ESP32-D0WD-V3`, alvo `esp32dev`.
- Rádios: dois **DX-LR32-900T22D**, relatados como de fábrica, sem configuração prévia pelo usuário.
- Os dois rádios responderam com versão interna **V1.2.4**.
- As placas são alimentadas por USB. Inicialmente, seus pinos 3V3 alimentavam os rádios; no S3, a alimentação era distribuída pela protoboard.
- O usuário informou teste de VIN para VCC do LoRa no gateway, sem mudança do sintoma. Não houve registro de medição da tensão nesse ensaio; portanto, ele não comprova um teste efetivo em 5 V.
- Foi informada medição de **3,28 V entre VCC e GND de um LoRa** durante a investigação do gateway. Não é uma medição simultânea dos dois módulos nem de transientes durante transmissão.
- O usuário confirmou antenas conectadas e distância aproximada de **40 cm** entre os rádios. A adequação elétrica das antenas/conectores à faixa de 915 MHz não foi validada por medição.

O manual técnico especifica VCC de 3,3–5,5 V, 5 V típico, sinais de comunicação de 3,3 V e cerca de 188 mA de transmissão nas condições tabeladas. A leitura de 3,28 V fica ligeiramente abaixo do mínimo nominal, mas, sem avaliar instrumento e variação sob carga, não identifica sozinha a causa da falha. Esses dados não tornam 5 V uma decisão para a alimentação final do nó. [Manual técnico, seções 1.4 e 2.3](https://manuals.plus/m/b087b010464009ab8cae0994d7e6f90bc91332c7ed308156b739d74bdd31cfcb).

### 2.2 Portas e velocidades

| Interface | Configuração usada |
| --- | --- |
| USB do S3 | `/dev/cu.usbmodem5CF70235711`, monitor em **115200 baud** |
| USB do gateway | `/dev/cu.usbserial-5B520925891`, monitor em **115200 baud** |
| ESP ↔ módulo LoRa, nos dois testes | **9600 baud, 8N1** |

As portas podem mudar ao reconectar dispositivos. A velocidade do monitor USB e a velocidade da UART do rádio são configurações distintas.

### 2.3 Pinagem do firmware de teste atual

| Sinal | ESP32-S3 | Gateway WROOM |
| --- | --- | --- |
| RX do ESP ← TXD do LoRa | GPIO39 | GPIO16 / RX2 |
| TX do ESP → RXD do LoRa | GPIO38 | GPIO17 / TX2 |
| M0 do LoRa | GPIO1 | GPIO19 |
| M1 do LoRa | GPIO2 | GPIO21 |
| AUX do LoRa | GPIO5 | GPIO18 |
| SHT interno SDA / SCL | GPIO8 / GPIO9 | Não utilizado neste firmware |
| SHT externo SDA / SCL | GPIO17 / GPIO18 | Não utilizado neste firmware |
| SPI SCK / MISO / MOSI | GPIO12 / GPIO13 / GPIO11 | Não utilizado neste firmware |
| CS ADXL / CS SD | GPIO10 / GPIO14 | Não utilizado neste firmware |
| RTC CE / IO / CLK | GPIO47 / GPIO40 / GPIO21 | Não utilizado neste firmware |
| ADC de bateria / CHRG | GPIO4 / GPIO15 | Não utilizado neste firmware |

Fonte: [pinos do S3](../src/sensor/pins.h) e [firmware do gateway](../src/comunicacao_wroom_test/main.cpp). Números são GPIOs, não posições no conector. Essa tabela não substitui a pinagem da bancada WROOM antiga.

### 2.4 Parâmetros de bancada

- Ciclo do S3: 60 segundos, sem deep sleep.
- ADXL: ODR configurada de 1600 Hz, janela de 16.000 amostras por eixo, aproximadamente 10 segundos; polling de FIFO no teste.
- ADXL: acesso direto a registradores, SPI_MODE3; configuração atual de SPI em 5 MHz, full resolution, ±2 g.
- SHT30: dois controladores I²C separados, 100 kHz, três tentativas de medição por sensor, conversão de 16 ms.
- SD: SPI configurado em 4 MHz; teste temporário de um registro de 24 bytes.
- Envio: um registro por quadro DATA, até quatro tentativas; espera de ACK de 2000 ms por tentativa concluída localmente.
- Gateway: estatísticas a cada 15 segundos, sem encaminhamento para cloud nesse ambiente.

São parâmetros do código de teste atual, não validação de política científica, frequência final de aquisição ou autonomia. A bancada WROOM anterior de 100 Hz continua sendo uma referência distinta.

## 3. Achados detalhados

### A01 — Velocidade incorreta do monitor USB

**Confirmado e corrigido no uso do terminal.** Foi usado `-b 11520`, produzindo caracteres ilegíveis. Com `-b 115200`, os logs passaram a ser legíveis. Isso não configura a UART interna do rádio.

### A02 — Estouro de pilha no processamento de vibração

**Defeito de firmware identificado e corrigido; sem recorrência do mesmo panic nos logs posteriores apresentados.**

```text
Debug exception reason: Stack canary watchpoint triggered (loopTask)
Backtrace: 0x42003a34 ... 0x42002224 ...
```

O backtrace do binário inicial foi associado ao cálculo em `BeeCoolerVib::compute`. Os buffers locais de FFT/PSD consumiam aproximadamente 11 KiB, excedendo a pilha de 8 KiB da tarefa utilizada naquela configuração.

Os buffers de trabalho foram movidos para armazenamento estático. A verificação de uso de pilha do cálculo registrou 192 bytes. Consequência: o cálculo **não é reentrante**; chamadas simultâneas de tarefas diferentes precisam ser impedidas. Não foram alterados os parâmetros de aquisição para resolver esse panic.

Fonte: [BeeCoolerVib.cpp](../lib/BeeCoolerVib/src/BeeCoolerVib.cpp) e [contrato da função](../lib/BeeCoolerVib/src/BeeCoolerVib.h).

### A03 — UART do rádio e UART USB estavam sendo confundidas

**Configuração de bancada ajustada e posteriormente confirmada por AT.** Os testes passaram a usar 9600 baud para ESP ↔ LoRa, mantendo o monitor USB em 115200. Nos dois módulos, `AT+BAUD` retornou `+BAUD=3`, correspondente a 9600 no guia serial.

A configuração compartilhada está em [BeeCoolerBenchConfig.h](../include/BeeCoolerBenchConfig.h). Essa alteração dos testes não reconfigurou os módulos nem alterou automaticamente os firmwares de produção, cujo alinhamento de UART continua sendo uma pendência antes de usá-los com estes rádios.

### A04 — Entrada AT exigiu o comando completo com CR/LF neste ensaio

**Correção de firmware confirmada em bancada nos dois módulos.**

A primeira implementação enviava somente `+++`. No gateway, houve sucesso quando a tentativa enviou `+++\r\n` inteiro:

```text
[LoRa AT TX] +++<CR><LF>
[LoRa AT RX] Entry AT
[LoRa AT TX] AT<CR><LF>
[LoRa AT RX] OK
```

A tentativa intermediária de compatibilidade tinha outro erro: receber `Power on` fazia o S3 pular a tentativa com CR/LF. Essa condição foi removida. A versão atual envia o comando completo desde a primeira tentativa e também ao sair de AT.

Nos logs mais recentes, ambos responderam às consultas e à saída:

```text
[LoRa AT RX] Exit AT
Power on
```

`Power on` sozinho não é uma resposta `OK` ao comando AT nem comprova comunicação entre rádios. Depois de `Exit AT`, ele é compatível com a reinicialização descrita pelo fabricante. Não deve ser interpretado automaticamente como queda de alimentação.

Fonte: [diagnóstico AT compartilhado](../include/BeeCoolerBenchRadio.h). O formato foi validado pelos logs do usuário; não foi inferido apenas das simulações.

### A05 — M0/M1 eram comandados pelo ESP com os rádios em SWITCH=0

**Incompatibilidade identificada e corrigida nos testes; a correção não resolveu, por si só, a falta de recepção RF.**

Os dois módulos retornaram `+SWITCH=0`. O manual técnico descreve M0/M1 como saídas do módulo nessa configuração. O driver anterior configurava os dois GPIOs do ESP como saídas, inicialmente HIGH, criando possibilidade de conflito entre saídas. [Manual técnico, seção 2.3.4](https://manuals.plus/m/b087b010464009ab8cae0994d7e6f90bc91332c7ed308156b739d74bdd31cfcb).

Foi acrescentada a opção `drive_mode_pins=false` ao driver, usada pelos dois firmwares de bancada. M0/M1 ficam como INPUT no ESP, sem pull-up; o teste não comanda sleep por esses GPIOs e continua usando AT para consultar o módulo.

Após a correção, o gateway mostrou `M0=1 M1=1 AUX=0 RX=1` e continuou respondendo AT. Com `SWITCH=0`, esses níveis não devem ser interpretados usando a tabela de seleção de modos por GPIO de `SWITCH=1`.

O comportamento padrão dos outros chamadores foi preservado. **Os firmwares de produção ainda precisam alinhar o uso de GPIO/AT e o baud rate com os módulos reais antes de serem usados nesta montagem.** Não foram gravados comandos para alterar permanentemente `SWITCH`, canal, chave ou potência.

Fonte: [driver LoRa](../lib/BeeCoolerLora/src/BeeCoolerLora.cpp).

### A06 — UART local funciona; recepção pelo ar permanece ausente

**Confirmado nos últimos logs.** Parâmetros consultados coincidem nos dois módulos:

| Parâmetro | Gateway e S3 |
| --- | --- |
| VERSION | V1.2.4 |
| BAUD | 3 |
| MODE | 0 |
| LEVEL | 2; HELP informa 2149 bps |
| CHANNEL / Frequency | 41 / 915150000 Hz |
| SLEEP | 2 |
| SWITCH | 0 |
| DRSSI | 0 |
| OPENKEY | 1 |
| MAC | ff,ff |
| CRC / IQ | 1(true) / 1(true) |
| Power | 22 dBm |

Valores copiados das respostas reais, inclusive CRC/IQ; não foram substituídos por valores de exemplos do manual.

No S3:

```text
[LoRa] TX local concluido; aguardando ACK do gateway...
[LoRa] timeout: nenhum ACK correspondente ao registro
[LoRa] ciclos confirmados=0/2
```

No gateway:

```text
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
```

Interpretação:

- `UART OK` confirma a comunicação ESP ↔ módulo durante o diagnóstico AT.
- `TX local concluído` indica envio de bytes pela UART e observação da transição AUX esperada pelo driver. Não mede RF irradiada nem prova entrega remota.
- `uart_bytes` conta recepção no gateway **após** a consulta AT. Zero nesse contador não contradiz as respostas AT anteriores.
- Não há evidência de quadros chegando ao parser nesse período. Portanto, ainda não há base para atribuir a falha a CRC32 do protocolo ou ao retorno do ACK.
- A coincidência dos parâmetros consultados não confirma todos os estados internos. Em particular, `OPENKEY=1` informa habilitação da chave, não seu valor. A igualdade das chaves não foi verificada; o guia informa que a chave não é consultável. Isso é uma pendência, não uma causa demonstrada. [Guia serial, seção 5.3.9](https://manuals.plus/m/a61c62f351554677e82e5972bf5eb419788493ecc1e0abb5af237f807c311a97.pdf).

Não houve registro de `[LoRa RX] OK` no gateway ou de `LoRa=ACK OK` no S3 até este relatório.

### A07 — Mau contato no S3 depende de pressão na placa

**Confirmado por observação física relatada pelo usuário.**

1. SCL do SHT interno não apresentava continuidade com GPIO9.
2. Ao pressionar o S3 mais fundo na protoboard, a continuidade aparecia.
3. O usuário relatou pressionar a placa quando os dois SHT30 voltaram a fornecer dados.

Isso identifica contato instável no caminho medido. Não identifica, sozinho, se o ponto exato é a mola da protoboard, o header, uma solda ou o encaixe do jumper, nem prova que toda falha de todos os periféricos tem a mesma causa.

A continuidade informada anteriormente não descarta um contato que muda com a posição. A montagem precisa manter contato sem pressão manual antes de testes de estabilidade.

### A08 — Diagnóstico I²C distingue ausência de sensor de dado inválido

**Instrumentação adicionada; ausência de resposta observada durante falhas.**

```text
[SHT30 EXTERNO I2C] inicio=OK presente=NAO endereco=0x00 | probe44=2 probe45=2
[SHT30 EXTERNO I2C] leituras_validas=0/3 falhas_comando=0 falhas_leitura=0 falhas_validacao=0
```

`inicio=OK` refere-se ao controlador I²C do ESP. Na versão instalada de `Wire`, o código 2 de `endTransmission()` representa NACK/ausência de confirmação na sondagem do endereço. Não é comprovação de que o sensor esteja energizado ou funcional.

Os contadores seguintes ficam em zero porque, sem detectar o endereço, o driver não tenta a medição. `endereco=0x00` significa que nenhum endereço foi encontrado; não significa que o SHT tenha mudado para 0x00. O valor 255 no probe significa “não testado”.

Houve também `requestFrom(): i2cRead returned Error -1` antes de resultados finais válidos. O resultado usa a mediana das leituras válidas dentre três tentativas: `OK` não implica que todas as tentativas tenham passado.

No último ciclo 2 completo: interno **37,01 °C / 46,74 %RH**, externo **33,72 °C / 49,24 %RH**. São exemplos de leitura recebida, não calibração dos sensores ou caracterização térmica do ambiente.

Os pinos, frequência I²C, comando de medição e tempo de conversão não foram alterados para contornar as falhas. Foram adicionados resultados de inicialização/probe e contadores ao [driver compartilhado](../src/sensor/Sht30Pair.cpp).

### A09 — RTC apresenta data incorreta e comunicação/contagem instáveis

**Não resolvido.** Foram observados:

- Calendário em `2099-01-01`, às vezes avançando e às vezes parado.
- Falha de leitura de retorno após tentativa de ajuste com a hora de compilação.
- Oito registradores lidos como `FF` ou como `00` em diferentes inicializações.
- No último log, segundos em `34` repetidos entre ciclos, com falha do teste de avanço.

Quando todos os bytes são `FF`, os valores derivados `CH=1`, `modo12h=1` e `WP=1` não devem ser tratados como uma configuração confiável do RTC: a leitura inteira pode estar inválida.

A mensagem foi corrigida de `RTC=OK` para **`CONTANDO/HORA NAO VALIDADA`** quando o calendário é representável e os segundos avançam. Isso evita apresentar 2099 como horário correto, mas **não corrige o relógio**.

Limitação ainda presente: um calendário representável e crescente, mesmo com data errada, pode ser usado como `rec.ts` sem `kRtcInvalid`. Quando o teste falha, o registro usa segundos desde o boot e marca `kRtcInvalid`. Não interpretar todos os timestamps como datas reais sem observar essa distinção.

Não foram confirmadas sincronização externa, precisão, retenção com alimentação principal desligada ou integridade da bateria de backup.

### A10 — microSD funciona intermitentemente

**Não resolvido.** O cartão foi identificado com aproximadamente **29.818 MB**. Em alguns ciclos, passou por escrita de 24 bytes, fechamento, reabertura, leitura e comparação. Em outros, ocorreram:

```text
f_mount failed: (3) The physical drive cannot work
sdCommand(): Card Failed! cmd: 0x11
ff_sd_status(): Check status failed
fopen(...tmp) failed
```

Houve ainda um ciclo com erro de baixo nível seguido por teste final `SD OK`. O OK representa o resultado da transação verificada naquele ciclo, não ausência de erros nem estabilidade do cartão/barramento.

O teste usa arquivo temporário exclusivo, não formata o cartão e remove apenas o arquivo criado por ele. A montagem passou a ser mantida após sucesso; falhas de I/O provocam tentativa de remontagem no ciclo seguinte. Essa mudança não eliminou a intermitência observada.

Ainda não foi isolada a causa entre alimentação, contato, cartão, módulo SD e operação do SPI compartilhado. Não há evidência suficiente para condenar ou formatar o cartão.

### A11 — ADXL voltou a completar a captura, mas isso não valida toda a bancada

**Funcionamento observado em múltiplos ciclos.** Exemplo recente:

```text
[ADXL345] DEVID=0xE5 | amostras=16000/16000 por eixo | duracao=10235 ms | overruns=0
[VIBRACAO] RMS=63.29 mg | pico=598.54 mg | segmentos=61
```

Em uma condição anterior de falha generalizada, o DEVID não correspondeu a 0xE5. Nos últimos ciclos completos, a identificação e a janela voltaram a ser válidas. Isso confirma aquisição nessas execuções, não qualidade científica das features, montagem mecânica final ou estabilidade de todos os periféricos.

O resumo do ADXL exige janela completa, processamento válido e zero overruns para apresentar OK. Os parâmetros experimentais de vibração permanecem distintos da bancada WROOM antiga.

### A12 — ADC de bateria e CHRG ainda não representam uma medição validada

**Não validado.** Foram impressos valores entre aproximadamente 0,788 V e 2,517 V em ciclos diferentes, com divisor 5 e ganho 1. Essas leituras são do caminho ADC/divisor de bateria; **não são medições diretas da alimentação VCC do LoRa**.

O aviso `Preferences ... nvs_open failed: NOT_FOUND` aparece ao tentar abrir o namespace de calibração em modo somente leitura. O código mantém os valores padrão quando essa abertura falha. O aviso não demonstra, por si só, falha do LoRa ou necessidade de apagar a flash.

`CHRG GPIO15=HIGH` pode ocorrer com o sinal desconectado, porque o teste usa pull-up interno. Não confirma carregamento, bateria presente ou alimentação estável.

Pendente: confirmar o circuito de medição efetivamente conectado e comparar o ADC com multímetro. Nenhuma calibração de bateria foi comprovada durante esta sessão.

### A13 — Reinício durante captura tem causa ainda desconhecida

**Evento observado; causa não confirmada.** No último log houve uma nova inicialização após `[adxl] capturando janela de 10 s...`, com `rst:0x1 (POWERON)`.

Não apareceu o panic de stack canary nesse trecho. Foi perguntado se o usuário pressionou reset/reconectou o USB ou se o reinício foi espontâneo; não houve resposta até esta consolidação. Não classificar esse evento como brownout, watchdog ou recorrência do estouro de pilha sem evidência adicional.

### A14 — O envio não exige todos os sensores em OK

**Confirmado pelo fluxo do firmware e pelos logs.** O S3 forma o registro com os dados disponíveis e flags de falha. Sensores ausentes usam sentinelas; RTC inválido usa tempo desde o boot; falha no SD não impede tentar LoRa.

O envio é suspenso quando a inicialização/consulta AT não é confirmada ou quando AUX não fica ocioso. Depois da confirmação local do rádio, houve quatro tentativas de envio mesmo com RTC e SD em falha.

Valores como `t=-32768`, `rh=65535` e bandas `255` representam dados inválidos/ausentes no formato do registro; não são leituras físicas.

### A15 — Limites de alimentação, proximidade e interpretação das fotos

**Hipóteses e correções de interpretação, sem causa RF demonstrada.**

- O ensaio relatado com VIN não solucionou o problema. Sem tensão medida nesse ensaio, não concluir que “5 V foram testados e descartados” em condições controladas.
- A distância de 40 cm foi informada. Foi proposto afastar os rádios alguns metros para comparação, mas não foi apresentado resultado desse ensaio. O fabricante recomenda separação nos testes; o valor sugerido de alguns metros é uma escolha de bancada, não um limite garantido. [Guia serial, seção 5.3.6](https://manuals.plus/m/a61c62f351554677e82e5972bf5eb419788493ecc1e0abb5af237f807c311a97.pdf).
- As fotos do gateway foram consultadas localmente; não estão incluídas neste relatório versionado.
- A inspeção visual levantou uma suspeita de VCC/GND invertidos. O usuário esclareceu a orientação e a ligação correta; **essa suspeita foi retirada**. Não registrar inversão nem dano por polaridade como fatos.
- Não foi comprovado dano a nenhum módulo LoRa, defeito de antena, queda de tensão sob TX ou saturação do receptor por proximidade.

## 4. Correções e instrumentação presentes no código

| Área | Alteração | Evidência de validação e limite |
| --- | --- | --- |
| Vibração | Buffers estáticos em `compute` | Builds/testes; mesmo panic não reapareceu nos logs apresentados; função não reentrante |
| UART LoRa | Constante de bancada de 9600 baud | `BAUD=3` nos dois módulos |
| AT | Comando completo `+++\r\n`, logs de TX/RX, reconhecimento de linhas | Entrada, consultas e saída confirmadas nos dois rádios |
| M0/M1 | Opção de não dirigir GPIOs; testes usam INPUT no ESP | Teste simulado e uploads; RF continuou sem recepção |
| SHT30 | Resultados de begin/probe e contadores | Logs distinguem ausência no endereço de falha de leitura/validação |
| RTC | Leitura de registradores, conferência de ajuste e mensagem de hora não validada | Falhas visíveis; data/contagem não corrigidas |
| SD | Teste temporário de escrita/leitura; retenção de montagem após sucesso | Sucessos e falhas reais registrados; intermitência persiste |
| Enlace | ACK por quadro/registro, contadores de tentativas e bytes no gateway | Nenhum ACK confirmado até o último log |

Arquivos principais: [S3](../src/comunicacao_s3_test/main.cpp), [gateway](../src/comunicacao_wroom_test/main.cpp), [configuração de bancada](../include/BeeCoolerBenchConfig.h), [AT](../include/BeeCoolerBenchRadio.h), [driver LoRa](../lib/BeeCoolerLora/src/BeeCoolerLora.cpp), [SHT30](../src/sensor/Sht30Pair.cpp), [RTC](../src/sensor/Ds1302Clock.cpp) e [vibração](../lib/BeeCoolerVib/src/BeeCoolerVib.cpp).

As mudanças foram locais. Nenhum commit ou push foi feito nesta investigação. Duas entradas de stash da transição anterior de `main` para `dev` permaneciam presentes na consolidação: `WIP main restante antes de testar dev` e `WIP antes de mudar para dev`.

## 5. Compilações, testes e ambiente de desenvolvimento

| Verificação executada durante a sessão | Resultado |
| --- | --- |
| Builds dos dois ambientes `comunicacao_*_test` após a correção de M0/M1 | Sucesso |
| Uploads manuais mais recentes do usuário | Sucesso nos dois ESPs; mensagens confirmam a versão com M0/M1 em INPUT |
| Builds de `sensor` e `sensor_fast` após a mudança de assinatura do driver LoRa | Sucesso |
| Testes existentes `pio test -e native`, após instrumentação dos SHT30 e ajuste AT | 22 casos passaram |
| Simulações AT em programa temporário no host | 12 cenários passaram; incluem respostas ausentes, aviso de boot, resposta inválida e saída de AT |
| Simulação do controle M0/M1 no host | Verificou ausência de escrita nos pinos em modo AT, rejeição de sleep por GPIO, timeout AUX e preservação do modo GPIO padrão |
| Builds adicionais de `gateway` e `gateway_fault` | Não concluídos: `No space left on device` no Mac |
| `git diff --check` | Sem erros de whitespace nas verificações realizadas |

Os programas de simulação foram criados em `/private/tmp/beecooler-at-check` e `/private/tmp/beecooler-mode-check`; são artefatos temporários, não testes permanentes versionados. Simulações e compilações não comprovam o enlace físico.

O disco chegou a cerca de 123 MiB livres. Foram limpos apenas artefatos regeneráveis dos builds adicionais, preservando os binários de bancada, fontes e fotos. A última consulta antes deste relatório mostrou cerca de 226 MiB livres. Espaço livre é uma medição daquele momento, não garantia para a próxima compilação.

Também houve anteriormente demora em instalação de toolchain pelo PlatformIO e em carregamento de tarefas da extensão. O uso da CLI permitiu gravar e monitorar as placas; a causa da lentidão da extensão/download não foi diagnosticada. O ambiente Python `.venv` não impediu a execução pelo caminho absoluto do PlatformIO.

## 6. Pendências e sequência sugerida

| Ordem | Ação | Evidência esperada | Situação |
| --- | --- | --- | --- |
| 1 | Estabilizar encaixe/contatos do S3 com a alimentação desligada | Continuidade e funcionamento sem pressionar a placa | Pendente |
| 2 | Registrar se o reinício durante ADXL foi manual ou espontâneo | Resposta do usuário e, se espontâneo, novo log do evento | Pendente |
| 3 | Executar ciclos consecutivos com a montagem estável | Sensores sem desaparecimento e sem reinícios inesperados | Pendente |
| 4 | Comparar recepção com os rádios separados por alguns metros, mantendo demais condições | Mudança ou permanência de `uart_bytes=0`, com logs dos dois lados | Proposto, sem resultado |
| 5 | Se zero bytes persistir, preparar teste exclusivo de rádio com texto simples nos dois sentidos | Recepção bruta por direção, independente de sensores e de parser BeeCooler | Proposto, ainda não implementado |
| 6 | Se necessário, investigar alimentação sob carga, antenas/conectores e compatibilidade de configuração, inclusive chave | Medições e ensaios controlados, uma variável por vez | Não concluído |
| 7 | Isolar RTC, SD e medição de bateria após estabilizar contatos | Data correta/avanço/retenção; SD repetível; ADC comparado ao multímetro | Não concluído |

Não alterar silenciosamente canal, chave, potência, baud rate, alimentação ou pinagem para “tentar fazer funcionar”. Registrar o estado anterior, a alteração e o resultado de cada ensaio. A estabilidade do encaixe deve preceder conclusões de estabilidade do software.

### Critério de confirmação do enlace

No gateway, deve aparecer recepção de quadro válido e aumento dos contadores. No S3, deve aparecer ACK correspondente ao registro enviado:

```text
Gateway: [LoRa RX] OK: quadro validado com CRC32
S3:      [LoRa] OK: gateway recebeu registro ... e respondeu ACK
S3:      [RESUMO] ... LoRa=ACK OK
```

Um ACK confirma a entrega daquele registro. A validação de estabilidade exige repetir o ensaio sem manipular os contatos e registrar tentativas, perdas e duração. Quantidade e duração de um ensaio de aceitação ainda não foram definidas.

### Modelo para registrar a próxima rodada

```text
Data/hora:
Firmware/commit e alterações locais:
Montagem/protoboard:
Alimentação e tensão medida em cada módulo:
Antenas e distância:
Houve pressão na placa, movimento ou reset manual?:
Gateway — UART AT / uart_bytes / quadros_ok / acks_tx:
S3 — UART AT / tentativas / ACKs confirmados:
SHT interno / SHT externo / ADXL / RTC / SD:
Alteração única deste ensaio:
Resultado e arquivos de log:
```

## 7. Referências

- [Pinagem documentada](pinos.md) e [ambientes PlatformIO](../platformio.ini).
- [Manual técnico do DX-LR32-900T22D, versão 2.0 — documento do fabricante em espelho](https://manuals.plus/m/b087b010464009ab8cae0994d7e6f90bc91332c7ed308156b739d74bdd31cfcb).
- [Guia serial do DX-LR32-900T22D, versão 2.0 — documento do fabricante em espelho](https://manuals.plus/m/a61c62f351554677e82e5972bf5eb419788493ecc1e0abb5af237f807c311a97.pdf).
- Logs, medições e esclarecimentos do usuário durante esta sessão, distinguindo comandos executados de propostas ainda sem resultado.

## 8. Atualização com os resultados posteriores — consolidada em 05/10/2026

As seções anteriores preservam o retrato de 03/10. Os logs posteriores mudaram a conclusão sobre o enlace: **houve entregas confirmadas por ACK, mas a estabilidade ainda não foi estabelecida**.

- Antes da troca dos contatos, uma execução chegou a **4/30 ciclos confirmados**; houve sucessos nas tentativas 1 e 3, intercalados com ciclos sem ACK.
- Depois de substituir o encaixe do LoRa do nó sensor na protoboard por jumpers fêmeas, uma execução apresentou **6/6 ciclos confirmados na primeira tentativa**, com RTT entre aproximadamente **758 e 774 ms**. Isso reforça a hipótese de contato instável na montagem anterior. Não comprova estabilidade permanente nem isola todos os possíveis pontos de falha.
- Uma execução posterior apresentou **0/9 ciclos confirmados**. O usuário informou que o rádio do gateway ainda estava na protoboard, enquanto o rádio do S3 usava jumpers fêmeas. A conexão do gateway é o próximo ponto proposto para isolamento; sua troca ainda não foi confirmada. Os contadores de execuções distintas não devem ser somados como um único ensaio controlado.
- O ensaio informado a cerca de **15 m, entre andares**, continuou sem novos ACKs naquele período. Distância e obstáculos mudaram juntos; esse resultado não identifica sozinho a causa.
- Os SHT30 e o teste de escrita/leitura do SD passaram nos ciclos recentes apresentados. Isso melhora a evidência de funcionamento dessas partes, sem constituir ensaio de duração suficiente para garantir estabilidade.
- O RTC alternou entre registradores zerados e leituras em **2099-01-02**. Nos ciclos 8 e 9 mais recentes, o timestamp avançou exatamente 60 s. A data continua incorreta e o ajuste de escrita/leitura não foi validado.
- No ciclo 6 da execução com 6/6 ACKs, o pico calculado de vibração foi **30215,60 mg**; o usuário informou ter movimentado o próprio ADXL. O movimento explica uma alteração no sinal, mas o valor numérico extremo permanece sem validação dos dados brutos e do processamento.
- A medição de bateria continua sem validação por multímetro e não representa a tensão de alimentação do LoRa.

**Próximos passos:** estabilizar e comparar as conexões do gateway, coletar suas estatísticas durante falhas, confirmar modelo/alimentação/ligações do RTC e validar sua escrita, contagem e retenção. Manter firmware e configuração constantes ao comparar mudanças físicas. Não foi demonstrado defeito permanente nos módulos ou antenas.
