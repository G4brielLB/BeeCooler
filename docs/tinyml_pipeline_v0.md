# BeeCooler — Pipeline de TinyML v0 (atuação e sensor virtual)

**Versão:** v0 — 27/11/2026 · **Código:** `../analysis/ml/` (`run_all.py`) · **Dados:** `coleta_dados/coleta1e2/BEE/N01/` (campo, tiúba, quintal, 22–27/11/2026; fora do repositório, caminho em `BEECOOLER_DATA` ou `config.DATA_ROOT`)
Convenção: `✅` fechado · `🟡` provisório · `🔴` pendente de medição ou validação.

> **Escopo dos dados.** Todos os números abaixo vêm de **uma colônia de *Melipona fasciculata* (tiúba), um nó (N01), ~4,5 dias úteis, tempo estável e ensolarado**, no quintal da casa de um integrante. Não extrapolar para *Apis mellifera*, para o apiário da Fase 3, nem para dias nublados ou chuvosos.

---

## 1. Objetivos

| # | Objetivo | Formulação adotada |
|---|---|---|
| A | Indicar **quando** acionar a bomba de nebulização e **por quanto tempo** | Previsão de T_in em +30 min (regressão, formulação B da seção 31 de `energia_revisao_v6.md` (documentação de engenharia da equipe, fora do repositório)), com uma camada de regras de histerese decidindo a bomba |
| B | Avaliar se o sensor **externo** consegue estimar a T_in (eliminar o SHT30 interno) | Regressão T_in(t) usando só o sensor externo e a hora (sensor virtual) |

A nebulização é **externa**: a água é borrifada no ambiente ao redor da caixa, não dentro dela. Para o sensor virtual isso é favorável, porque o sensor externo enxerga o efeito da atuação e a T_in responde através da caixa (ver §8).

## 2. Revisão rápida de literatura (limiares) 🟡

Não foi encontrado limiar de estresse térmico publicado **específico para tiúba**. Referências úteis, lidas só pelo resumo ou trecho (conferir o texto completo antes de citar no relatório):

| Fonte | O que informa |
|---|---|
| Embrapa, X Jornada Científica — condições ambientais de colônias de tiúba ([PDF](https://www.alice.cnptia.embrapa.br/alice/bitstream/doc/1188581/1/CondicoesAmbientais-p25AnaisXJornadaCientifica.pdf)) | T interna média de 32,2–33,4 °C contra 32,3–34,2 °C fora; RH interna variou de 31,8 a 65,7 % (controle de umidade mais difícil) |
| Embrapa, IV Jornada Científica 2018 ([1](https://www.alice.cnptia.embrapa.br/alice/bitstream/doc/1120945/1/QuartaJornadaCientifica00016.pdf), [2](https://www.alice.cnptia.embrapa.br/alice/bitstream/doc/1121122/1/QuartaJornadaCientifica2018300054.pdf)) | Picos da T interna entre 11:20 e 12:40 (coerente com o pico de ~12h observado aqui) |
| *M. scutellaris*, termogênese da cria ([FAPESP](https://bv.fapesp.br/pt/publicacao/270628)) | Atividade metabólica da cria elevada entre 25 e 32,5 °C, sugerindo uma faixa de desenvolvimento mais baixa; regulação **heterotérmica**, não homeostática |
| *M. subnitida*, Apidologie 2020 ([DOI](https://doi.org/10.1007/s13592-020-00811-z)) | Tolerância ao calor das campeiras **cai com o tempo de exposição** — a duração acima do limiar importa, não só o pico |
| Meliponíneos australianos ([PubMed](https://pubmed.ncbi.nlm.nih.gov/37677867/)) | CTmax de 43–44,5 °C; *T. carbonaria*: 95 % de mortalidade após 1 h a 42 °C |

**Leitura para o projeto:** a colônia observada passa de 37 °C todos os dias entre ~11h e ~17h, acima da faixa típica relatada para tiúba (32–33 °C) e da faixa de cria de *Melipona* (até ~32,5 °C), mas abaixo da CTmax de adultos (~43 °C). Limiares adotados como ponto de partida, em `config.LABEL`: **t_on = 36 °C, t_off = 35 °C** (histerese de 1 °C), mínimo de 20 min ligada e 20 min desligada, alarme em 40 °C. Fica `🟡` até haver validação com um meliponicultor ou com o texto completo das fontes.

## 3. Dados e controle de qualidade

`rec.bin` = registros estendidos de 64 B (`ExtendedRecord`, CRC16 CCITT-FALSE), 1 a cada 10 min. Todos os 770 registros passaram no CRC.

| Problema | Registros | Tratamento |
|---|---|---|
| Modo TEST_FAST (ciclo de 30 s, bancada antes da instalação, T_in ≈ 24 °C) | 80 (seq 1–80) | Excluído ✅ |
| **SHT30 externo com falha**: RH_ext = 0,00 % e T_ext implausível (13–27 °C com T_in a 37 °C) | 25 (24/11 09:00; 27/11 06:50–13:30) | Excluído ✅ — **o firmware não marcou `kShtOutFail`** (o sensor respondeu com CRC válido) |
| Lacunas e *cold boots* | 22/11 00:00→11:50; 24/11 07:40→10:10; 27/11 manhã | Quebram o segmento ✅ |
| Vibração com estouro de FIFO ou pico saturado (madrugada de 24/11) | 10 entre os utilizáveis | Mascarado ✅ |
| `vbat` sempre 0xFFFF | 770 | Sem dado de bateria 🔴 |

Resultado: **665 registros utilizáveis em 2 segmentos contínuos** (22/11 11:50 → 24/11 07:40 e 24/11 10:10 → 27/11 04:50).

**Achados de firmware** (para o time de firmware, não corrigidos aqui):
1. **Falha do SHT30 externo sem flag.** RH = 0 com T plausível passa pela checagem. Sugestão: tratar RH ≤ 0,5 % (ou T_ext < T_in − 10 °C de dia) como `kShtOutFail`.
2. **As 3 leituras do SHT30 não são independentes.** Em 99,7 % dos ciclos, a 1ª leitura repete o valor do ciclo anterior e a 2ª é idêntica à 3ª. A "mediana de 3" não filtra ruído, e **o ruído do sensor não é estimável com estes dados** 🔴. Investigar o modo de medição e a temporização em `src/sensor/Sht30Pair.cpp`.

## 4. Comportamento observado

Figuras em `../analysis/ml/out/figs/` (`01_serie_qc`, `02_perfil_horario`, `03_inercia`, `04_vibracao`, `05_rotulos`).

- **T_in**: ~30–31 °C de madrugada; sobe das 9h às 12h; **pico de 38,0 ± 0,4 °C perto do meio-dia e patamar de 37–37,5 °C até ~16h** em todos os dias completos.
- **Noite**: T_in fica ~2,3 °C acima da T_ext (isolamento da caixa somado ao calor da colônia). **De dia**: T_in fica abaixo da T_ext.
- **T_ext**: sobe até 43–47 °C entre 10h e 11h e cai para 39–40 °C ao meio-dia. Esse é o padrão de **sol direto no sensor externo**, seguido de sombra: ele mede "aquele ponto", não a temperatura do ar. O modelo aprende essa instalação; mudar o sensor de lugar exige re-treino.
- **Inércia térmica**: T_in segue melhor uma versão suavizada da T_ext do que a instantânea (EMAs de 30–240 min; `03_inercia.png`).
- **Vibração**: RMS de 24–26 mg com deriva diária que acompanha a temperatura; energia por banda plana ao longo do dia. **Nenhum sinal de atividade da colônia visível nos resumos** — coerente com a pendência P6 (SNR do ADXL345). A vibração não entra nos modelos v0. A análise do `raw.bin` (espectros) fica pendente 🔴.

## 5. Rótulos (supervisão fraca)

Não houve atuação na coleta, então **não existe rótulo verdadeiro**. O rótulo é o que a regra de histerese teria feito sobre a T_in medida, dentro da janela diurna 07–18h (D34):

| t_on (t_off = t_on − 1) | Ligada por dia completo | Início mediano |
|---|---|---|
| 35 °C | 442 min | 10:40 |
| **36 °C (padrão)** | **422 min (~7 h)** | **11:00** |
| 37 °C | 395 min | 11:30 |

Resultados que importam:
- **Todos os episódios terminam às 17:50, cortados pela janela diurna**, não pela temperatura: às 18h a T_in ainda está em ~35,5 °C. A janela de D34 precisa ser revista para o caso de calor (🟡: estender até ~19h, ou desligar pela temperatura).
- A duração de ~7 h/dia é o cenário **sem resfriamento**. Com a nebulização ativa, a T_in deve cair antes e os ciclos ficarão mais curtos. **A dose real só pode ser calibrada com o ensaio da bomba** 🔴 (Fase 2).
- "Por quanto tempo" sai do próprio autômato: liga em t_on (ou pela previsão), desliga em t_off, respeitando os tempos mínimos.

## 6. Modelos e validação

Validação **deixando um dia de fora**: treina com os outros dias e testa no dia que ficou de fora. As EMAs precisam de 60 min para convergir; amostras nesse aquecimento ficam fora do treino e da avaliação. MAE e P95 do erro absoluto, em °C.

### 6.1 Previsão de T_in em +30 min (objetivo A)

Entradas: T_in, tendências de T_in em 10 e 30 min, RH_in, T_ext, tendência de T_ext em 30 min, EMAs de T_ext (60 e 240 min) e hora em seno/cosseno.

| Modelo | MAE | P95 | Máx. | MAE diurno |
|---|---|---|---|---|
| persistência (baseline) | 0,300 | 0,94 | 2,01 | 0,403 |
| tendência 30 min (baseline) | 0,193 | 0,57 | 1,34 | 0,280 |
| linear (Ridge) | 0,128 | 0,34 | 1,14 | 0,170 |
| árvore (prof. 5) | 0,159 | 0,46 | 1,13 | 0,224 |
| **MLP 10-16-8-1 (α = 1)** | **0,115** | **0,32** | 1,27 | **0,150** |

A MLP é ~10 % melhor que o linear no MAE e é o modelo exportado. O ganho sobre o linear é pequeno, e com mais dados a escolha é refeita automaticamente.

### 6.2 Sensor virtual: T_in só com o sensor externo (objetivo B)

Entradas: T_ext, RH_ext, EMAs de T_ext (30/60/120/240 min), EMA de RH_ext (60 min) e hora em seno/cosseno.

| Modelo | MAE | P95 | Máx. | MAE diurno |
|---|---|---|---|---|
| média por hora (baseline) | 0,423 | 0,98 | 1,77 | 0,323 |
| **linear (Ridge)** | **0,196** | **0,52** | 1,34 | 0,236 |
| árvore (prof. 5) | 0,229 | 0,59 | 1,34 | 0,209 |
| MLP 16-8 | 0,457 | 1,38 | 6,84 | 0,595 |

O linear é o modelo exportado. A MLP **sobreajusta**: com tão poucos dias ela extrapola mal nos inícios de segmento. O erro máximo do linear (1,34 °C) ocorre perto das lacunas.

### 6.3 Efeito na decisão da bomba (previsões fora da amostra)

| Condução da bomba | Minutos ligada (5 episódios) | Precisão / revocação vs. ideal | Antecipação |
|---|---|---|---|
| Regra reativa sobre T_in medida | 1990 | — | 0 |
| Ideal antecipada (com a T_in real em +30 min) | 2080 | 1 / 1 | — |
| **max(T_in, previsão MLP)** | **2080** | **1,00 / 1,00** | **30 min em toda subida com o modelo ativo** (média de 18 min; os dois zeros são o 22/11, em que os dados começam já quente, e o 24/11, logo após um reinício) |
| Sensor virtual linear (sem SHT30 interno) | 2000 | 0,995 / 1,00 vs. regra reativa | ~0 |

### 6.4 Embarque no ESP32-S3

`../lib/BeeCoolerML/src/beecooler_tinyml.h` é gerado por `run_all.py` e não deve ser editado à mão. É C99 puro, sem biblioteca, e contém:

- **Estado em RTC RAM** (`bcml_state_t`, < 100 B; D32): 4 EMAs, 1 EMA de RH, as últimas 4 leituras e o estado da bomba. As EMAs usam passo irregular, α = 1 − e^(−Δt/τ), então funcionam com os ciclos noturnos de 30 min (D35). O estado zera num *cold boot*.
- **Modelos**: previsão MLP com 321 parâmetros e sensor virtual linear com 10, em float32 e em INT8 (pesos INT8 com escala por camada).
  - Pesos: ~1,5 kB em float32 ou ~0,6 kB em INT8.
  - Diferença entre INT8 e float: até 0,003 °C.
- **Camada de decisão** `bcml_decide()`: histerese, tempos mínimos e janela diurna. Enquanto as EMAs aquecem (< 60 min após um reset), decide só pela T_in medida.
- **Paridade**: `parity_test.c` compara C e Python nos 665 registros: diferença máxima de 8·10⁻⁶ °C e nenhuma divergência na decisão da bomba (gcc no host) ✅. Ainda não foi rodado no S3 🔴.

O tamanho fica bem abaixo da faixa-alvo de 20–100 mil parâmetros (seção 34 de `energia_revisao_v6.md`). Com tão poucos dados, um modelo maior só sobreajustaria. O custo de inferência é desprezível diante dos 50 ms orçados.

### 6.5 Variante de firmware `sensor_ml`

`pio run -e sensor_ml` (ou `sensor_ml_fast` para bancada) compila o mesmo `src/sensor_prod/main.cpp` com `-DBEE_TINYML=1`. Sem essa flag, o `sensor_prod` não muda.

| Peça | O que faz |
|---|---|
| `lib/BeeCoolerML/` | `BeeCoolerML.h/.cpp` traduz o registro canônico para o modelo e trata os casos que o treino não cobre; `beecooler_tinyml.h` é o cabeçalho gerado |
| Estado | `RTC_DATA_ATTR BeeCoolerML::State gMl` (128 B), zerado no *cold boot* e numa lacuna > 60 min, como os segmentos do treino |
| Momento | Depois de medir T/RH e antes de gravar no *ring*, em todo ciclo. À noite só atualiza as EMAs; a previsão roda na janela 07–18h (D34) |
| Saída | Bit `kPumpRequest` (bit 14) nas flags do registro canônico, que chega à nuvem sem mudar o formato de 24 B, e uma linha `ml seq=… motivo=… bomba=… t_in=… prev30min=… virtual=… h=… us=…` no `events.log` e na serial |
| Atuador | `pumpCommand()` é só o ponto de ligação; o ESP32-C3 com a bomba (ESP-NOW) é da Fase 2 |

Casos de sensor, em `BeeCoolerML::step()`:

| Situação | Comportamento |
|---|---|
| Os dois SHT30 válidos, EMAs prontas | Previsão + histerese sobre max(T_in, T_in prevista) |
| < 60 min após um reset | Decide só pela T_in medida (`aquecendo`) |
| SHT30 externo inválido (RH ≤ 0,5 %, como em 27/11) | Sem modelo; decide pela T_in medida (`ext_invalido`) |
| SHT30 interno inválido | **A T_in passa a ser a do sensor virtual** (`int_virtual`) e a decisão segue com ela, sem previsão |
| Os dois inválidos, ou relógio inválido | Bomba desligada (`sem_sensor`, `sem_relogio`) |

Teste nativo: `pio test -e native -f test_tinyml`. Ele confere a lib contra os vetores do Python (`test/test_tinyml/vectors.h`, gerado) e os casos da tabela acima. Rodou no host com g++ e um substituto mínimo do Unity: 7/7 ✅. Ainda falta rodar pelo PlatformIO e compilar o `sensor_ml` para o S3 🔴.

## 7. Re-treino com mais dados

O pipeline foi feito para ser realimentado: novos dias em `coleta_dados/` → `run_all.py` → QC, validação, escolha do modelo e cabeçalho C refeitos. O que mais melhora os modelos, em ordem:
1. **Dias com outro tempo** (nublado, chuva, frente fria). Hoje todos os dias são quase iguais, e até a média por hora já acerta 0,42 °C.
2. **Ensaio da bomba** (ligar em horários conhecidos), para medir o efeito na T_in e calibrar a dose.
3. Mais colônias ou nós, para avaliar a generalização do sensor virtual entre caixas.

## 8. Limites e pendências

| ID | Item | Estado |
|---|---|---|
| ML-1 | Limiares t_on/t_off sem referência específica para tiúba | 🟡 validar com meliponicultor ou literatura completa |
| ML-2 | Rótulos sem atuação real; duração da dose não calibrada | 🔴 ensaio da bomba (Fase 2) |
| ML-3 | ~4,5 dias de tempo estável: métricas otimistas para outros climas | 🔴 mais coleta |
| ML-4 | Sensor virtual × nebulização: o modelo não viu T_ext e RH_ext sob nebulização. O SHT30 externo pode molhar ou condensar (a falha de 27/11 parece com isso) | 🔴 conferir no ensaio; definir a posição do sensor em relação ao bico |
| ML-5 | Janela diurna de D34 (07–18h) corta a atuação com T_in ainda ~35,5 °C | 🟡 rever D34 para o modo calor |
| ML-6 | Vibração sem sinal da colônia nos resumos | 🔴 analisar `raw.bin` (P6) |
| ML-7 | Falha do SHT30 externo sem `kShtOutFail`; leituras do SHT30 não independentes | 🔴 firmware |
| ML-8 | A escolha da MLP usou uma varredura de hiperparâmetros na mesma validação: leve otimismo | 🟡 validação aninhada quando houver mais dias |
| ML-9 | Lib e cabeçalho validados só no host; `sensor_ml` ainda não compilado | 🔴 `pio run -e sensor_ml`, `pio test -e native -f test_tinyml`, depois rodar no S3 e medir o tempo de inferência (`us=` no log) |
| ML-10 | Bit 14 das flags (`kPumpRequest`) é novo no registro canônico | 🟡 registrar na Arquitetura; nuvem e dashboard hoje guardam as flags sem interpretar esse bit |

Sobre eliminar o sensor interno: **viável como estudo** (MAE de 0,2 °C e mesma decisão de bomba nestes dias), mas **não recomendado como decisão** antes de ML-3 e ML-4. Na atuação, o sensor externo enxerga a nebulização, o que reduz o risco do laço fechado. Em troca, ele fica exposto a água e sol, e é justamente a falha dele que tira o sistema do ar (27/11).
