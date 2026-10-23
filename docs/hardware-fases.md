# Hardware Completo
## Produtos Nacionais

* Sensor Temperatura e Umidade A Prova D'Água SHT30 I2C - 2 unidades - R$ 127,60 (63,80 por unidade); OBS: AliExpress acha por menos de 30 reais

* ESP32 Wroom Devkit - 2 unidades - R$ 74,00 (R$ 37,00 por unidade)

* Módulo Sensor Acelerômetro 3 eixos ADXL345 GY-291 SPI ou I2C - 1 unidade - R$ 33,50

* Bateria 18650 2600mah 3,7v Flexgold Recarregável - 1 unidade - R$ 24,69

* Módulo Adaptador MicroSd Tf Card - 1 unidade - R$ 10,92

* Motor para Bomba de Pulverizador Elétrico Pem P20 Kawashima - 1 unidade - R$ 70,20

## Produtos Importados

* ESP32-S3-N16R8 (16MB Flash | 8MB PSRAM) - 1 unidade - R$ 33,48

* Cartão de Memória Somambulist 16GB - 1 unidade - R$ 16,19

* Kit Módulo Lora LR32 868/915MHz (LR32-A Module + Glue Stick Antenna + USB Cable + USB to TTL Adapter Board) - 2 unidades - R$ 103,10 (R$ 51,10)

* Módulo TPS63020 (Buck Booster 3,3V) - 1 unidade - R$ 14,92

* ESP32-C3 - 1 unidade - R$ 13,63

* Kit Painel Solar (Mini Painel Solar de Silício Monocristalino 6V 210MA 1.25W + Carregador Solar CN3065 + Suporte para Bateria 1S 18650 + 2 Conectores de Fios Rápido CH2 + 3 Cabos JST + 1 Display de Nível de Bateria 1S) - 1 unidade - R$ 36,12

OBS: Os valores dos produtos importados já estão contidos frete e impostos de importação (ICMS)

# Kit Colmeia

Efetivamente para o nó Colmeia serão utilizados:

Para Alimentação Energética:

* Kit Painel Solar (Placa Voltaica + CN3065)

* Bateria 18650

* Módulo TPS63020 (Buck Booster 3,3V)

Sensores

* 1 módulo sensor acelerômetro ADXL345

* 2 módulos sensores SHT30 (1 externo e 1 interno) - O sensor externo pode ser compartilhado para diversas colmeias

Estrutura

* 1x ESP32 Wroom ou 1x ESP32-S3 (a definir)

* Módulo MicroSD e Cartão MicroSD (Por enquanto, apenas para salvar dados brutos, talvez não seja utilizado em produção)

* 1x Módulo Lora com Antena

# Kit Atuador

Efetivamente para o nó Atuador (mecanismo de nebulização) serão utilizados:

Para Alimentação Energética:

* Bateria SLA/AGM 12V 7Ah (recarga manual no protótipo, sem painel solar por enquanto)

* Módulo LM2596 (Step-down, ~12V para ~5V, alimentando o ESP32-C3)

Acionamento

* Motor para Bomba de Pulverizador Elétrico Pem P20 Kawashima

* Driver de acionamento: MOSFET IRF3205 + transistores BC337/BC327 + diodo de proteção 1N5408

Estrutura e Proteções

* 1x ESP32-C3

* Estrutura hidráulica do mecanismo de nebulização: mangueiras, bicos de nebulização e válvula de bypass/retorno ao reservatório

* Sensor de tensão 0-25V (monitoramento da bateria SLA)

* Chave-boia (impede acionamento com reservatório vazio)

* Fusível inline de proteção

Comunicação nó sensor (colmeia) → nó atuador: ESP-NOW (curto alcance, ~10-15m)

# Fases do Projeto

## Fase 0 - Prototipação Inicial

**Objetivo:** validar os sensores e o funcionamento geral do protótipo de forma bem simples, utilizando apenas o básico dos hardwares. Questões energéticas (bateria, carregador solar, autonomia) ainda não entram nessa fase - apenas o orçamento dessa parte foi levantado, sem uso efetivo.

**O que foi feito:** protótipo inicial do dashboard; ligações elétricas dos sensores; protocolo de coleta e envio periódico dos dados para o gateway; protocolo de comunicação utilizando acks e retransmissão.

**Hardware utilizado:**

* 2x ESP32 Wroom (um como nó sensor, outro como gateway/central)

* 2x Sensores Temperatura e Umidade A Prova D'Água SHT30

* 1 módulo sensor acelerômetro ADXL345

## Fase 1 - Coleta de Dados Pura + Atuação Funcionando

Fase realizada com abelhas tiúba, em um ambiente "controlado" (local aberto, com uma colmeia em local próximo e de fácil acesso para qualquer intervenção imediata). Possui duas ramificações, que acontecem em paralelo:

### Ramo 1 - Coleta de dados (colmeia tiúba)

**Objetivo:** ligar os sensores e iniciar a coleta contínua de dados, salvando-os no cartão SD, já com o nó conectado à bateria e ao carregador solar. Ao fim da coleta de 7~15 dias, os dados serão extraídos para analisar o comportamento e as variações, decidindo a partir disso o algoritmo de processamento a ser usado (MPL, AIoT, etc.).

**Hardware utilizado:**

* 1x ESP32-S3 - nó sensor

* 2x Sensores SHT30 (1 interno e 1 externo)

* 1 módulo sensor acelerômetro ADXL345

* Bateria 18650 + Kit Painel Solar (Placa Voltaica + Carregador CN3065 + acessórios)

* Módulo TPS63020 (Buck Booster 3,3V)

* Módulo MicroSD + Cartão MicroSD (armazenamento dos dados brutos coletados)

### Ramo 2 - Circuito e estrutura do atuador

**Objetivo:** ligar todo o circuito elétrico e montar a estrutura do atuador, validando o envio de um comando para o ESP atuador e o acionamento do mecanismo de nebulização.

**Hardware utilizado:** Kit Atuador completo (ver seção acima) + ESP32-C3 (atuador), motor da bomba PEM P20 Kawashima, driver de acionamento (IRF3205 + BC337/BC327 + 1N5408), bateria SLA/AGM 12V 7Ah + LM2596, estrutura hidráulica, chave-boia, sensor de tensão e fusível.

## Fase 2 - MVP

**Objetivo:** consolidar a Fase 1 com a atuação já completa e conectada ao nó sensor, ainda no ambiente controlado. Os dados coletados passam a ser disponibilizados para visualização em um dashboard, e o processamento/decisão passa a ser feito na própria borda (edge), definindo se o mecanismo de resfriamento térmico por nebulização deve ser ativado (e por quantos minutos). Ao final dessa validação, tem-se um MVP autônomo e validado, com uma noção melhor de autonomia e funcionamento e dos demais desafios que surgirem.

**Hardware utilizado:** integração completa do Kit Colmeia (nó sensor) e do Kit Atuador:

* Kit Colmeia: ESP32-S3, 2x SHT30, ADXL345, Bateria 18650 + Kit Painel Solar, TPS63020, MicroSD

* Kit Atuador: ESP32-C3, motor da bomba + driver de acionamento, bateria SLA/AGM 12V + LM2596, estrutura hidráulica, proteções

* Comunicação nó sensor → atuador via ESP-NOW (curta distância no ambiente controlado)

* Dashboard para visualização dos dados coletados

## Fase 3 - Teste em Ambiente Real

**Objetivo:** testar o MVP em ambiente real - apiário na Embrapa, com o nó a cerca de 300m de distância do gateway, longe de qualquer energia elétrica ou comunicação convencional (por isso o uso de LoRa), ainda com uma única colmeia. Diferente da Fase 2 (onde o LoRa pode até ser usado para testar o firmware, mas em poucos metros de distância), aqui o LoRa é validado de fato em longa distância. 

Como o apiário é de abelhas com ferrão (Apis mellifera), local e espécie diferentes da Fase 1/2, os dados de temperatura interna e comportamento (vibração) precisam ser recoletados. As abelhas com ferrão sofrem mais com temperatura/umidade por serem criadas em ambiente mais crítico (sertão), diferente das abelhas sem ferrão, geralmente criadas próximas a sedes, em locais sombreados. Além disso, essa espécie possui comportamentos ativos de resfriamento da colmeia, que potencialmente podem ser capturados pelo sensor de vibração - que, desde o início, também funciona como detector de eventos externos (abertura da colmeia, vandalismo por animais/humanos, chuva forte, etc.).

**Hardware utilizado:** o mesmo Kit Colmeia + Kit Atuador da Fase 2, acrescido de:

* 1x Módulo Lora LR32 com Antena no nó sensor (substituindo/complementando o ESP-NOW de curto alcance usado nos testes de bancada)

* 1x ESP32 Wroom + Módulo Lora LR32 com Antena no gateway/central, para recepção a longa distância

## Fase 4 - O Sonho (Trabalhos Futuros)

**Objetivo:** fase final, a se pensar como trabalho futuro (fora do escopo da disciplina, por questões de tempo), com o objetivo de monitorar mais de uma colmeia ao mesmo tempo, utilizando o MVP já validado, mas com adaptações para escalar o projeto para múltiplas colmeias.

**Hardware utilizado (a definir/escalar):**

* N x ESP32 (Wroom/S3) - um por colmeia, cada um com seu próprio sensor SHT30 interno e módulo ADXL345, enviando os dados para o "ESP central"

* 1x ESP32 "central" agregador, com Módulo Lora LR32 para envio ao gateway

* 1x Sensor SHT30 externo compartilhado, ligado apenas ao "ESP central" (ao invés de um sensor externo por colmeia)

* Réplicas do kit de alimentação energética (Bateria 18650 + Kit Painel Solar + TPS63020) por colmeia (ou geral - a definir)

* Estrutura do Kit Atuador replicada ou compartilhada entre colmeias (a definir)