# BeeCooler — Dashboard v5 (dados reais: ESP-NOW e LoRa)

Especificação para a equipe **implementar o dashboard**. Esta versão substitui as anteriores (`BeeCooler_Dashboard_Spec.md` e `BeeCooler_Completo.md`), que foram escritas para um desenho futuro (LoRa, bateria, RSSI, vibração em faixas) que **ainda não existe no repositório**.

- **Protótipo navegável:** https://claude.ai/artifact/7sCjpXhci7iegKxvtNBYAw
- **Implementação de referência:** `beecooler_painel_v5.html` (HTML único, sem dependências, com os dados reais já embutidos)
- **Repositório:** https://github.com/G4brielLB/BeeCooler

## 1. Meta

**Comparar os dados internos da colmeia com os externos (ambiente)** e acompanhar a saúde da comunicação sensor → central. Tudo na tela deve servir a essa comparação.

## 2. Decisões de escopo (já combinadas)

| Decisão | Detalhe |
|---|---|
| Duas páginas | **Telemetria** (dados coletados) e **Sistema** (comunicação e rede) |
| Interno e externo na mesma página | Nunca separar em páginas diferentes |
| Temperatura e umidade no mesmo card/gráfico | Um para o interno e outro para o externo, com eixo duplo |
| Gráficos com mais de uma variável | Interno × externo no mesmo gráfico, cores diferentes |
| Somente modo claro | Sem tema escuro |
| Uma colmeia só | Sem seletor de colmeia nesta fase |
| Sem emoji | Ícones SVG; o logo é uma abelha desenhada em SVG |
| Vocabulário | "interno" e "externo" |
| Funciona em PC e celular | Layout responsivo, interação por mouse e por toque |

## 3. O que o hardware entrega hoje

Fonte: firmware em `src/` e `lib/` (ESP32, PlatformIO), coletor em `tools/gateway_collector.py`.

- **Sensores:** 2 × SHT30 (temperatura e umidade, interno e externo) e 1 × ADXL345 (aceleração x, y, z).
- **Transporte:** **ESP-NOW** com protocolo próprio v1 (cabeçalho de 20 bytes, CRC-32, fragmentação, ACK, até 5 tentativas). **Não é LoRa.**
- **Lote:** 60 registros, 1 amostra a cada 500 ms (30 s por lote).
- **Central:** imprime na serial; o coletor grava três CSVs em `data/`.

### 3.1 `records.csv` (uma linha por amostra)

| Coluna | Significado |
|---|---|
| `session_id` | Sessão de coleta |
| `received_at` | Hora em que a central **recebeu o lote** (ISO-8601, -03:00) |
| `batch_id`, `sample_index` | Lote e posição (0 a 59) |
| `sensor_ms` | Milissegundos do sensor quando a amostra foi lida |
| `elapsed_ms` | Tempo desde o início do lote |
| `internal_temp_c`, `internal_rh_pct` | SHT30 interno |
| `external_temp_c`, `external_rh_pct` | SHT30 externo |
| `accel_x_g`, `accel_y_g`, `accel_z_g`, `accel_magnitude_g` | ADXL345, em g |

### 3.2 `batches.csv` (uma linha por lote)

`session_id, received_at, batch_id, batch_start_ms, bytes, records, interval_ms, duplicates`

### 3.3 `gateway_stats.csv` (contadores **acumulados por sessão**, a cada ~30 s)

`session_id, received_at, valid_data_packets, unique_fragments, duplicate_fragments, malformed_packets, crc_failures, acks_transmitted, completed_batches, incomplete_batches_replaced, sequence_gaps, receive_queue_drops`

### 3.4 Dados disponíveis para teste

Cerca de 31 mil registros em 5 sessões (03/09 a 16/09/2026), em 4 dias: 03, 04, 09 e 16/09. Sessões de 17 min a 2 h. Temperatura interna entre 24,9 e 38,7 °C. Sem leituras `nan`. Central: 66 lotes completos na última sessão, sem falhas de CRC, lacunas ou perdas de fila.

### 3.5 Ainda NÃO existe nos dados

Bateria (tensão e nível), RSSI e SNR, tentativas de ACK por fragmento (o firmware imprime `ack_attempt_1..5` na serial do sensor, mas o coletor não grava), vibração em bandas de frequência (a 2 amostras por segundo não há como fazer análise espectral útil). Esses itens **não devem aparecer** no dashboard até existirem na coleta; ver seção 11.

## 4. Pré-processamento dos dados (ponto crítico)

**Armadilha:** o `received_at` de um registro é a hora de chegada do **lote inteiro**, não a hora da amostra. Os 60 registros de um lote têm `received_at` quase iguais. Usar esse campo como eixo de tempo comprime 30 s de dados em um único instante.

**Reconstrução do horário de cada amostra:**

```
t_amostra = max(received_at do lote) − (max(sensor_ms do lote) − sensor_ms) / 1000
```

(o lote é identificado por `session_id` + `batch_id`).

Em seguida, agregar em **baldes de 5 s** (média de temperatura e umidade; para a aceleração: média, máximo e desvio-padrão). Isso reduz 31 mil linhas a cerca de 3 mil pontos.

```python
import pandas as pd
E = pd.Timestamp('1970-01-01', tz='UTC')
tt = lambda s: (pd.to_datetime(s, utc=True) - E).dt.total_seconds()
r = pd.read_csv('data/records.csv'); r['rt'] = tt(r.received_at)
g = r.groupby(['session_id', 'batch_id'])
r['t'] = g.rt.transform('max') - (g.sensor_ms.transform('max') - r.sensor_ms) / 1000
r['k'] = (r.t // 5 * 5).astype(int)
a = r.groupby('k').agg(
    ti=('internal_temp_c', 'mean'), ri=('internal_rh_pct', 'mean'),
    to=('external_temp_c', 'mean'), ro=('external_rh_pct', 'mean'),
    am=('accel_magnitude_g', 'mean'), mx=('accel_magnitude_g', 'max'),
    sd=('accel_magnitude_g', 'std')).fillna({'sd': 0}).dropna()
```

Os contadores da central (`gateway_stats.csv`) usam o `received_at` direto (uma linha a cada ~30 s), sem reconstrução.

Fuso: os dados vêm em -03:00 (Brasília). O dashboard exibe sempre o horário de Brasília, independentemente do fuso do aparelho.

## 5. Estrutura da tela

```
Cabeçalho:  [logo abelha] BeeCooler / "Monitoramento da colmeia"          "Último dado: dd/mm hh:mm"
Barra fixa (sticky):
   Abas:     Telemetria | Sistema
   Período:  [Último dia] [Última semana] [Último mês] [Dia específico] + seletor de data
   Dias:     "Dias com dados:" botões 03/09 04/09 09/09 16/09 (só em Dia específico)
Texto do período: "Período: ... até ..."
Conteúdo da página
Rodapé: fonte dos dados e o que ainda não é coletado
```

- A barra fixa respeita a área segura do celular (`env(safe-area-inset-top)`).
- O período vale para todos os gráficos da página.

## 6. Controle de período

Os dados são históricos (setembro) e a coleta é por sessões curtas, então **os períodos contam a partir do último dado recebido**, não de "agora".

| Opção | Janela | Balde do gráfico | Quebra de linha se lacuna maior que |
|---|---|---|---|
| Último dia | 24 h até o último dado | 30 s | 90 s |
| Última semana | 7 dias até o último dado | 10 min | 30 min |
| Último mês | 30 dias até o último dado | 1 h | 3 h |
| Dia específico | 00:00 a 23:59 do dia escolhido (Brasília) | 30 s | 90 s |
| Personalizado | ver 6.1 | 30 s, 10 min ou 1 h conforme o tamanho | 3 × o balde |

Regras:

- O **eixo X cobre só o intervalo com dados** dentro da janela (do primeiro ao último ponto), não a janela inteira. Sem isso, uma sessão de 30 minutos ficaria espremida em um eixo de 24 h.
- **Lacunas** (sem dados entre sessões) aparecem como **quebra na linha**, nunca como linha ligando pontos distantes nem como zero. Segmento de um único ponto vira um círculo.
- O seletor de data aceita só datas entre o primeiro e o último dia com dados. Os botões "Dias com dados" levam direto aos dias úteis.
- Período sem dados: cartão "Sem dados neste período", com o intervalo disponível e um botão "Ver o último dia com dados".
- Rótulos do eixo X (5, igualmente espaçados): `HH:mm` se o intervalo for até 36 h; `dd/MM HH:mm` até 10 dias; `dd/MM` acima disso.

### 6.1 Período personalizado

Quinto botão do controle de período. Abre um cartão com:

| Controle | Opções |
|---|---|
| Granularidade | **Por dia** (campos de data) ou **Por mês** (campos de mês) |
| De / Até | data inicial e final (por dia) ou mês inicial e final (por mês: do dia 1 do primeiro mês ao último dia do segundo) |
| Horário | **Dia inteiro**; **Horas do último dia registrado**; **Mesmas horas em cada dia do período** |
| Das / Às | horas inicial e final (só quando há filtro de horário) |

Regras:

- Se a data final for anterior à inicial, trocar as duas.
- Datas e meses só aceitam o intervalo que tem dados.
- **Horas do último dia registrado:** ignora as datas e mostra apenas o intervalo de horas escolhido no último dia com dados.
- **Mesmas horas em cada dia do período:** mantém só as amostras cuja hora do dia (em Brasília) está entre "das" e "às", em todos os dias do intervalo. Se "das" for maior que "às", o intervalo atravessa a meia-noite (ex.: 22:00 às 04:00).
- O filtro de horário vale para as duas páginas (telemetria e contadores da central).
- Balde do gráfico: 30 s se o intervalo tiver até 1,5 dia; 10 min até 10 dias; 1 h acima disso. Dias sem amostras no horário escolhido aparecem como lacunas.
- O texto do período descreve a seleção, por exemplo: "Período: 03/09 00:00 até 16/09 23:59, das 06:00 às 18:00 em cada dia do período".

## 7. Página Telemetria

### 7.1 Cards de resumo (último valor do período)

| Card | Conteúdo | Campos |
|---|---|---|
| Interno | Temperatura (°C) e Umidade (%), com etiqueta "Na faixa ideal" / "Acima do ideal" / "Abaixo do ideal" | `internal_temp_c`, `internal_rh_pct` |
| Externo | Temperatura (°C) e Umidade (%) | `external_temp_c`, `external_rh_pct` |
| Diferença (interno − externo) | Temperatura (°C, com sinal) e Umidade (p.p., com sinal), mais a frase "Interno X °C mais quente e Y p.p. mais úmido que o externo" | diferenças dos campos acima |

O título da seção mostra o horário do valor usado. Barra superior colorida: laranja no Interno, azul no Externo, cinza na Diferença.

### 7.2 Gráficos

| # | Título | Séries | Eixos | Faixa de referência (verde) |
|---|---|---|---|---|
| 1 | Interno: temperatura e umidade | temp. interna (contínua), umidade interna (tracejada) | esq. °C, dir. % | 32 a 36 °C |
| 2 | Externo: temperatura e umidade | temp. externa (contínua), umidade externa (tracejada) | esq. °C, dir. % | — |
| 3 | Temperatura: interna × externa | interna (laranja), externa (azul) | °C | 32 a 36 °C |
| 4 | Umidade: interna × externa | interna (laranja, tracejada), externa (azul, tracejada) | % | 50 a 75 % |
| 5 | Aceleração: média e pico | `accel_magnitude_g` média e máximo do balde | g | — |
| 6 | Intensidade da vibração | desvio-padrão de `accel_magnitude_g` no balde | g | — |

Sobre a vibração: em repouso a magnitude fica perto de **1 g** (gravidade). A informação útil é a **variação** em torno disso, por isso a intensidade é o desvio-padrão dentro de cada balde. Ao re-agregar para baldes maiores: média das médias, máximo dos máximos e raiz da média dos quadrados dos desvios.

## 8. Página Sistema

### 8.1 Cards (acumulado das sessões que tocam o período)

Para cada sessão no período, usar a **última linha** de `gateway_stats` dentro da janela (os contadores já são acumulados) e somar entre sessões.

| Card | Campo |
|---|---|
| Lotes completos | `completed_batches` |
| Pacotes válidos | `valid_data_packets` |
| ACKs transmitidos | `acks_transmitted` |
| Duplicados | `duplicate_fragments` |
| Falhas de CRC e malformados | `crc_failures` + `malformed_packets` |
| Lacunas e perdas de fila | `sequence_gaps` + `receive_queue_drops` |

O último card mostra "Comunicação sem falhas no período" (verde) quando CRC, malformados, lacunas e perdas somam zero; caso contrário, "Houve falhas: veja as sessões".

### 8.2 Gráficos

| # | Título | Cálculo | Unidade |
|---|---|---|---|
| 1 | Pacotes por minuto (válidos e ACKs) | diferença entre linhas consecutivas **da mesma sessão** dividida pelo tempo em minutos; a primeira linha de cada sessão é ignorada | pct/min |
| 2 | Lotes completos (acumulado na sessão) | `completed_batches` direto; volta a zero em nova sessão | lotes |

### 8.3 Tabela "Sessões de coleta"

Colunas: início, duração (min), registros, lotes, pacotes válidos, duplicados (duplicados do lote + fragmentos duplicados), falhas (CRC + malformados + lacunas + perdas). Linhas das sessões dentro do período ficam destacadas. A tabela rola na horizontal no celular.

## 9. Padrão de todos os gráficos

- **Cartão:** título com ícone, linha de dica, SVG, legenda abaixo.
- **Cor por local:** interno = laranja `#c2410c`, externo = azul `#2563eb`. **Estilo por variável:** temperatura = linha contínua, umidade = tracejada, em todos os gráficos.
- **Eixo duplo** quando há duas unidades (°C e %), com a unidade escrita no topo de cada eixo. O eixo esquerdo é da primeira série.
- **Faixa ideal por medida:** cada medida interna tem seu intervalo ideal (temperatura interna 32 a 36 °C; umidade interna 50 a 75 %, ambos a validar). O gráfico desenha a faixa de cada medida **visível**, no eixo dessa medida e com o texto "Faixa ideal X–Y unid." dentro dela. A primeira faixa é verde preenchida; a segunda (ex.: umidade no gráfico com eixo duplo) é um retângulo tracejado na cor da série. Medidas externas não têm faixa. A faixa sempre entra na escala do eixo.
- **Legenda clicável (isolar medida):** um botão por série, com amostra da linha, nome, valor atual e `mín · méd · máx`. Clicar em uma medida **deixa só ela** no gráfico (as outras somem e a escala se ajusta); clicar de novo nela **traz todas de volta**. Clicar em outra medida enquanto uma está isolada isola a nova. Ao isolar uma medida que estava no eixo direito, ela passa para o eixo esquerdo. O botão da medida isolada fica destacado e há uma dica sob a legenda.
- **Interação:** mouse ou dedo mostra uma linha vertical e, na dica acima do gráfico, o horário e o valor de todas as séries visíveis no ponto mais próximo. Tratar `pointermove` **e** `pointerdown`, para o toque funcionar. O SVG usa `touch-action: pan-y` para a rolagem vertical da página continuar funcionando sobre o gráfico.
- **Escalas:** automáticas, com 10 % de folga; a faixa de referência sempre entra na escala.

## 10. Design e responsividade

| Item | Valor |
|---|---|
| Tema | somente claro (`color-scheme: light`) |
| Fundo | `#f6f5f1`; card `#ffffff`, borda `#e4e1d8`, raio 12 px |
| Texto / secundário | `#1f2328` / `#6b7280` |
| Destaque (aba ativa, marca) | `#c98a00` |
| Interno / Externo | `#c2410c` / `#2563eb` |
| Positivo / alerta | `#15803d` / `#92400e` sobre `#fef3c7` |
| Fonte | do sistema (`system-ui`) |
| Números dos cards | 32 px (28 px em telas pequenas) |
| Ícones | SVG, traço de 2 px, 20 px: abelha (logo), hexágono, termômetro, gota, vibração, sinal, servidor, check, fila, comparação |
| Grade | cards de resumo a partir de 210 px; gráficos a partir de 420 px (1 coluna no celular, 2 no PC) |
| Largura máxima | 1100 px, centralizado |
| Celular (até 600 px) | abas ocupam a largura toda; botões de período rolam na horizontal; toque mínimo de 44 px |
| Área segura | `viewport-fit=cover` e `env(safe-area-inset-*)` |

## 11. Evolução prevista (quando o hardware e a coleta crescerem)

Itens que só entram no dashboard quando existirem nos dados; a estrutura já comporta:

1. **Bateria** (tensão e nível): gráfico com eixo duplo V e %, na página Sistema.
2. **RSSI e SNR**: gráfico de sinal, também no Sistema.
3. **Tentativas de ACK (1 a 5):** o firmware já as imprime (`ack_attempt_1..5`); falta o coletor gravá-las. Entrará como quadro de barras no Sistema.
4. **Vibração em bandas de frequência:** exige amostragem bem acima de 2 Hz.
5. **Dados ao vivo:** hoje o dashboard lê dados embutidos. A opção mais simples é servir os CSVs (ou um JSON equivalente) a partir do coletor (`gateway_collector.py` já tem um servidor HTTP local de teste). Uma API em nuvem só faz sentido depois que o protocolo definitivo estiver fechado.

O desenho em nuvem (Lambda, DynamoDB, rotas `/v1/...`) do `BeeCooler_API.md` parte de **outro formato de registro** e **não corresponde** ao que o repositório entrega hoje. Não implementar essa API antes de alinhar o formato.

## 12. Pendências e decisões em aberto

1. **Faixas de referência** (32 a 36 °C e 50 a 75 %) são propostas, sem validação de apicultor. Os dados reais chegam a 38,7 °C.
2. **Origem dos dados ao vivo** (item 5 da seção 11): CSV servido pelo coletor, arquivo estático ou API.
3. **Gravar as tentativas de ACK** no coletor, para a página Sistema ficar completa.
4. **Fuso horário:** o painel fixa Brasília. Confirmar se é o desejado.
5. **Período relativo ao último dado:** quando a coleta for contínua, trocar para relativo a "agora".
6. **Diário de campo** (existiu no primeiro protótipo): decidir se volta como terceira página.
7. **Nome/identificação da colmeia:** hoje há uma só, sem identificação na tela.

## 13. Checklist de aceitação

- [ ] Duas páginas (Telemetria e Sistema), com barra fixa e controle de período funcionando nas duas.
- [ ] Interno e externo juntos na mesma página; temperatura e umidade no mesmo card, com eixos separados.
- [ ] Gráficos de comparação: interno em laranja, externo em azul.
- [ ] Horário das amostras reconstruído a partir de `sensor_ms` (seção 4); nenhuma sessão aparece comprimida em um instante.
- [ ] Períodos: último dia, semana, mês e dia específico (seletor de data e botões dos dias com dados), contados a partir do último dado.
- [ ] Lacunas aparecem como quebras na linha, nunca como zero.
- [ ] Período sem dados mostra a mensagem e o botão para o último dia com dados.
- [ ] Clicar em uma medida isola só ela; clicar de novo restaura todas; a escala e o eixo se ajustam.
- [ ] A área verde mostra a faixa ideal da medida visível, com rótulo "Faixa ideal X–Y"; a umidade usa faixa tracejada quando as duas medidas aparecem.
- [ ] Período personalizado: por dia, por mês, e filtro de horário (último dia registrado ou mesmas horas em cada dia), inclusive intervalos que atravessam a meia-noite.
- [ ] Mouse e toque mostram horário e valores; a rolagem vertical continua funcionando sobre os gráficos.
- [ ] Sistema: totais corretos (última linha de cada sessão) e taxas calculadas só dentro da mesma sessão.
- [ ] Somente modo claro, sem emoji, ícones em SVG.
- [ ] Funciona em 360 px de largura e em tela larga de PC.
- [ ] O rodapé informa o que ainda não é coletado (bateria, RSSI/SNR, tentativas de ACK).

## 14. Segunda fonte de dados: teste LoRa de 40 min (05/10/2026)

O dashboard passou a ter um **seletor de origem dos dados** no cabeçalho: `LoRa · 05/10/2026` e `ESP-NOW · set/2026`. Cada origem tem seu próprio conjunto de campos, período disponível e texto de rodapé. Ao trocar de origem, o período volta para "Último dia" e as seleções de série são zeradas.

**Esclarecimento importante:** os dois logs enviados (`40min_gateway.md` e `40min_sensor_node.md`) mostram um firmware **diferente** do que está no repositório (ESP-NOW). Ele usa **LoRa** (módulo a 915 MHz, 9600 baud, UART), bateria, vibração RMS/pico e 4 bandas de frequência. Isso corresponde ao desenho do documento "Integração em Nuvem v1.1" e ao `BeeCooler_API.md`, que portanto **voltam a ser relevantes** para a fase LoRa.

### 14.1 Fontes e o que cada log traz

| Log | Conteúdo útil |
|---|---|
| `40min_gateway.md` | 49 linhas `BEEREC {json}` (um registro cada), linhas `[LoRa RX]` com `boot`, `quadro` e `tentativa`, e 224 linhas `[stats]` com contadores |
| `40min_sensor_node.md` | 40 ciclos com `[rec]`, diagnóstico (bateria, carga, SHT30, ADXL345, SD, RTC), `rtt` do ACK em µs e `[cycle] ativo por X ms` |

Os dois logs se cruzam pelo número de sequência (`seq 1–40`) da execução `boot 234`. O gateway também recebeu 9 registros de uma execução anterior (`boot 78`, `seq 8–16`) que não estão no log do nó.

### 14.2 Decodificação do registro (campos do `BEEREC`)

| Campo | Conversão | Exemplo |
|---|---|---|
| `t_in`, `t_out` | ÷ 100 → °C | 2819 → 28,19 °C |
| `rh_in`, `rh_out` | ÷ 100 → % | 4423 → 44,23 % |
| `vbat_mv` | ÷ 1000 → V | 810 → 0,810 V |
| `vib_rms`, `vib_peak` | ÷ 10 → mg | 343 → 34,3 mg |
| `bands[0..3]` | (valor − 200) ÷ 2 → dB | 239 → 19,5 dB |
| `attempt` | tentativa em que o ACK chegou (1 a 4) | 1 |
| `flags`, `seq`, `ts`, `node` | direto | `flags=0` |

A fórmula das bandas foi deduzida comparando o `BEEREC` com as linhas "bandas dB" que o próprio gateway imprime (239, 245, 248, 251 viram 19,50, 22,50, 24,00 e 25,50 dB). Conferir com quem escreveu o firmware.

### 14.3 Horário dos registros (ponto crítico)

- O `ts` vem do RTC do nó, que **não estava ajustado** no começo: 11 dos 49 registros têm `ts` em 2099 (epoch na casa de 4,07 bilhões).
- O RTC foi ajustado manualmente (UTC) entre o `seq 2` e o `seq 3` da execução `boot 234`; do `seq 3` em diante o `ts` é válido (22:49:25 UTC = 19:49:25 em Brasília).
- Para os 11 registros sem hora válida, o painel **estima** o horário: `seq 1` e `seq 2` ficam 120 s e 60 s antes do `seq 3` (ciclo de 60 s); a execução `boot 78` é posicionada pela diferença de `ts` em relação ao `seq 1` (o RTC de 2099 continuava contando entre as execuções). O rodapé da página informa quantos registros têm hora estimada.
- Janela resultante: 05/10/2026, de 19:38 às 20:26 (Brasília).
- Hora "precisa" depende do ajuste manual do RTC. Recomendação: sincronizar o RTC por NTP/gateway antes de testes longos.

### 14.4 Página Telemetria (LoRa)

- Cards Interno, Externo e Diferença; quatro gráficos de temperatura e umidade (iguais aos da outra origem).
- **Vibração: RMS e pico** (mg).
- **Energia por faixa de frequência:** 4 séries em dB (50–100, 100–200, 200–350 e 350–600 Hz), com a legenda que isola cada faixa.

### 14.5 Página Sistema (LoRa)

Cards (totais do teste inteiro, não filtrados pelo período): quadros válidos (49), ACKs transmitidos (49, 0 falhas), falhas de CRC (0), entrega na 1ª tentativa (100 %), tempo médio até o ACK (725 ms) e bateria.

Gráficos (filtrados pelo período): tensão da bateria; tempo até o ACK (ms); duração do ciclo ativo do nó (ms). Quadro "Tentativas até o ACK" (1ª a 4ª) e tabela "Execuções do nó".

### 14.6 Alertas sobre os dados do teste

1. **Bateria suspeita:** a leitura fica em ~0,8 V o teste todo (0,79 a 0,82 V). O próprio log diz "conferir com multímetro; ADC sozinho não confirma". O pino de carga (GPIO15) ficou em HIGH (pode indicar bateria desconectada). O painel mostra a leitura com a etiqueta "Leitura suspeita" e **não** desenha faixa ideal de tensão.
2. **RSSI/SNR indisponíveis:** o módulo foi consultado com `AT+DRSSI=0`, ou seja, o RSSI por pacote está **desligado**. Para ter gráfico de sinal, habilitar o RSSI no módulo e gravá-lo no registro.
3. **Hora do RTC** (ver 14.3).
4. **Amostra pequena:** 49 registros em 48 min (um por minuto). Os resultados de comunicação (100 % na 1ª tentativa, sem CRC) são de uma bancada curta, não de campo.
5. **1 byte descartado** no gateway no início da recepção (contador `bytes_descartados=1`), sem impacto nos quadros.

### 14.7 Checklist adicional

- [ ] O seletor de origem alterna entre LoRa e ESP-NOW e reinicia período e legendas.
- [ ] LoRa: gráficos de RMS/pico e das 4 bandas funcionam, com o botão que isola cada faixa.
- [ ] LoRa: o rodapé informa os registros com hora estimada e os dois alertas (bateria e RSSI).
- [ ] Os valores seguem as conversões da seção 14.2.
- [ ] Período personalizado e filtro de horário funcionam nas duas origens.
