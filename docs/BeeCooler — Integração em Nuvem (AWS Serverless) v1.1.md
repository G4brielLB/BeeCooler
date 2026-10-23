# BeeCooler — Integração em Nuvem (AWS Serverless) v1.1

Oct 2, 2026 · @victor

## Identificação

Esta versão substitui a v1.0 e passa a seguir a Arquitetura de Software v1.2: a nuvem recebe o mesmo registro canônico, com a mesma identidade `(node_id, seq)`, e não altera nada no nó sensor.

| Campo | Conteúdo |
| --- | --- |
| Projeto | BeeCooler — monitoramento e regulação térmica autônoma de colmeias |
| Disciplina | Tópicos em Sistemas Computacionais — UFPI/CCN/DC — 2026.2 |
| Escopo | Segundo salto da telemetria (central → nuvem), API, banco, dashboard PWA, segurança, custos, requisitos e casos de teste da integração |
| Fora do escopo | Nó sensor, protocolo LoRa e atuador: seguem a Arquitetura v1.2 sem mudanças |
| Documentos relacionados | AP1 (Fase 0), Arquitetura de Software v1.2, Briefing de Decisões v4.1, Revisão de Energia v6, `hardware.md` |
| Convenção de status | Fechada = decisão tomada; Proposta = valor provisório; Medir = depende de ensaio |

### Histórico de revisão

| Data | Versão | Mudança |
| --- | --- | --- |
| 02/10/2026 | 1.1 | Alinhamento à Arquitetura v1.2: chave `(node_id, seq)`, payload com o registro canônico, uplink por sessão, tarefas separadas na central, RF-11 sem polling de 2 s, Free Tier pós-julho/2025, novos RF/CT e rastreabilidade |
| 10/2026 | 1.0 | Primeira proposta, escrita antes da Arquitetura v1.1 |

## 1. Resumo executivo

A nuvem é o segundo salto do *store-and-forward* da seção 7.1 da Arquitetura v1.2: a central persiste, confirma ao nó e só depois encaminha à AWS. A malha de controle continua fechada no apiário, como exige a AP1 (seção 3.1.2); a nuvem só observa e, na Fase 2, sugere configurações.

A pilha é a mínima que atende: API Gateway HTTP API, uma Lambda em Python, DynamoDB, S3 privado e CloudFront para o PWA. Não há EC2, RDS, VPC, NAT ou IoT Core.

O que esta revisão corrige em relação à v1.0:

1. **Identidade do registro.** A chave do banco é `(node_id, seq)`, não o timestamp. Um `ts` repetido ou inválido (`rtc_invalid`) não sobrescreve mais dados em silêncio.
2. **Payload.** A central envia os 24 B do registro canônico v1 em base64, e a Lambda decodifica com o mesmo parser do repositório. Sentinelas viram ausência, nunca valor.
3. **Cadência.** O uplink acontece ao fim de cada sessão LoRa horária, mais um heartbeat da central a cada 15 min. Não existe lote de 30 min.
4. **Central.** Rádio e uplink rodam em tarefas separadas; o ACK LoRa nunca espera a nuvem.
5. **Dashboard.** Conteúdo completo do RF-11 (vibração, energia, enlace) e consulta a cada 60 s, no lugar dos 2 s herdados da Fase 0.
6. **Custos.** Contas criadas depois de 15/07/2025 usam o modelo de créditos; a v1.0 descrevia o modelo antigo de 12 meses.
7. **Rastreabilidade.** RF-10 e RF-11 revisados, RF-20 a RF-24 e RNF-12/13 novos, CT-11 a CT-13 no formato da AP1.

Com tráfego de uma colmeia, o consumo fica abaixo de 5% dos limites de requisições e custa centavos por mês. O nó sensor não muda: zero impacto no orçamento de \~51 mAh/dia da v1.2.

## 2. Decisões desta revisão

A numeração continua a da Arquitetura v1.2 (última: D53). A D54 revisa a seção 7.3 da v1.2, que previa nuvem só na Fase 2.

| # | Decisão | Status | Revisa | Consequência |
| --- | --- | --- | --- | --- |
| D54 | Telemetria vai para a AWS serverless **já na Fase 1** | Proposta | v1.2, seção 7.3 | Campanha CT-07 sem notebook ligado 15 dias; dashboard acessível de fora da casa |
| D55 | Coletor serial + SQLite fica como **ferramenta de bancada** (CT-03); a campanha usa só a nuvem | Proposta | v1.2, seção 7.3 | Injeção de falhas continua possível; o dataset oficial sai do DynamoDB + SD |
| D56 | Identidade na nuvem = `(node_id, seq)`: DynamoDB com `SK = seq` e GSI por `ts` | Fechada | v1.0, seções 13 e 16 | Reenvio grava o mesmo item; `ts` repetido não sobrescreve |
| D57 | Payload carrega o **registro canônico v1 (24 B) em base64**; a Lambda decodifica com o parser do repositório | Proposta | v1.0, seção 5 | Um registro, um parser (v1.2, seção 5.1); v1 → v2 vira um campo de versão |
| D58 | Uplink **ao fim de cada sessão LoRa**, mais heartbeat da central a cada 15 min | Proposta | v1.0: lote de 30 min | Dados no dashboard minutos após a sessão; "nó calado" ≠ "central fora" |
| D59 | Central com **tarefa de rádio** (prioridade alta) e **tarefa de uplink** separadas; a nuvem nunca entra no caminho do ACK | Fechada | v1.0, seção 48 | Handshake TLS não estoura o timeout de 400 ms nem polui RNF-03 e P-RTT |
| D60 | **HTTP API + Lambda**, não AWS IoT Core/MQTT | Proposta | v1.2, seção 7.3 ("MQTT/HTTP") | Sem X.509 por dispositivo nem conexão persistente; um POST por sessão basta |
| D61 | Dashboard consulta a API **a cada 60 s**; throttling no stage do API Gateway | Proposta | RF-11 (2 s) | \~1,3 milhão de GETs/mês por aba vira \~43 mil no pior caso |
| D62 | **Token Bearer por gateway**; HMAC com anti-replay fica para quando houver mais de uma central | Fechada | — | Simples no firmware; credencial restrita à API |
| D63 | **Diário de campo no PWA** (`POST /v1/journal`), com os mesmos campos da seção 11 da v1.2 | Proposta | D51 (CSV compartilhado) | Intervalos marcados nos gráficos e exportados junto com o dataset |
| D64 | Comandos remotos (UC-15, Fase 2) voltam na **resposta do POST** de ingestão | Proposta | — | Downlink sem MQTT: nuvem → central → byte "comando pendente" do ACK LoRa |

## 3. Arquitetura geral

A central é a fronteira: tudo à esquerda e acima dela funciona sem internet, e a AWS só recebe o que ela já persistiu e confirmou ao nó.

&#91;embedded content: arquitetura geral · zona local, AWS e usuário\]

O nó e o atuador conversam por ESP-NOW e a decisão de resfriar nunca passa pela nuvem. A central confirma ao nó depois de gravar no LittleFS e, em outra tarefa, encaminha a fila à API; o PWA é carregado do CloudFront e lê a mesma API.

## 4. Central: o segundo salto

Os passos 1 a 5 da seção 7.1 da v1.2 não mudam: validar, inserir por `(node_id, seq)`, persistir no LittleFS, atualizar `acked_up_to` e só então mandar o ACK. Esta seção detalha o passo 6, o encaminhamento à nuvem.

### 4.1 Duas tarefas, uma regra

| Tarefa | Núcleo / prioridade | Faz | Nunca faz |
| --- | --- | --- | --- |
| `task_radio` | core 1, alta | UART do DX-LR32, CRC, persistência, ACK, estado por nó (v1.2, seção 7.2) | Esperar Wi-Fi, DNS ou TLS |
| `task_uplink` | core 0, baixa | Wi-Fi, NTP, TLS, POST, heartbeat, limpeza da fila | Responder ao nó |

A regra: **o ACK LoRa nunca depende da nuvem.** As duas tarefas compartilham apenas a fila do LittleFS, protegida por mutex. Para reduzir disputa de CPU e de flash, a `task_uplink` começa a enviar \~5 s depois do último ACK da sessão do nó; a central conhece os slots (`hh:00 + 20 s + (node_id − 1) × 30 s`, v1.2 seção 6.5).

### 4.2 Fila de uplink

1. Cada registro aceito entra na fila com `seq`, os 24 B, `rx_time` (hora NTP da recepção), RSSI, SNR e `attempt` do frame.
2. O STATUS da sessão entra como item próprio (36 B, v1.2 seção 6.3).
3. A `task_uplink` monta um POST com tudo o que está pendente, até 32 KB de corpo; backlog maior vai em POSTs sucessivos.
4. Só um `200` remove os itens da fila. Um `400` move o lote para uma pasta de quarentena, para que um item malformado não trave a fila; um `401` suspende o uplink e acende o alerta no heartbeat seguinte.
5. Falha de rede ou `5xx`: novas tentativas em 5 s, 15 s e 60 s; depois, uma tentativa a cada 5 min, sem limite. Os dados continuam na fila.

Volume: \~40 B por registro na fila, 144 registros por dia, \~6 KB/dia por nó. Uma partição LittleFS de 1 MB guarda mais de 150 dias de um nó (P-LFS).

### 4.3 Heartbeat

A cada 15 min, mesmo sem dados de nó, a central envia `POST /v1/gateway/heartbeat` com: uptime, heap livre, RSSI do Wi-Fi, tamanho da fila, itens em quarentena, versão do firmware e se o NTP está válido. É isso que permite ao dashboard dizer "central sem contato" em vez de acusar o nó.

### 4.4 Relógio e TLS

A central sincroniza por NTP ao conectar e a cada 6 h. Sem NTP válido, ela não abre TLS (o certificado não pode ser validado) e envia epoch `0` nos ACKs, que o nó rejeita pela regra do UC-06. A validação usa o certificado raiz da Amazon Trust Services gravado no firmware, nunca `setInsecure()`.

O handshake TLS no WROOM precisa de dezenas de KB de heap, a ser medido junto com LoRa e LittleFS ativos (P-TLS).

### 4.5 Eventos (Fase 2)

Frames EVENT (RF-19) furam a fila: a `task_uplink` é acordada na hora, sem esperar o fim da sessão.

## 5. Contrato da API

Todas as rotas ficam sob `https://<id>.execute-api.<região>.amazonaws.com/v1`. As de escrita exigem token; as de leitura são públicas e protegidas por throttling (seção 9).

### 5.1 Rotas

| Método e rota | Quem chama | Token | Função | Fase |
| --- | --- | --- | --- | --- |
| `POST /v1/telemetry` | central | gateway | Ingestão de registros e STATUS de uma ou mais sessões | 1, 2 |
| `POST /v1/gateway/heartbeat` | central | gateway | Saúde da central (seção 4.3) | 1, 2 |
| `GET /v1/nodes` | PWA | — | Nós conhecidos, com `max_seq` e último contato | 1, 2 |
| `GET /v1/nodes/{node_id}/status` | PWA | — | Último STATUS decodificado + estado do enlace | 1, 2 |
| `GET /v1/nodes/{node_id}/readings?from=&to=` | PWA | — | Série por intervalo de `ts` (GSI), até 1.000 itens por página | 1, 2 |
| `GET /v1/gateways/{gateway_id}/status` | PWA | — | Último heartbeat | 1, 2 |
| `GET /v1/export?node_id=&from=&to=` | equipe | — | CSV dos registros (RF-10) | 1, 2 |
| `GET /v1/journal`, `POST /v1/journal` | PWA | equipe (POST) | Diário de campo (D63) | 1 |
| `POST /v1/nodes/{node_id}/commands` | PWA | equipe | Comando pendente para o nó (UC-15, D64) | 2 |

`node_id` é o inteiro de 1 a 254 do header LoRa (v1.2, seção 6.2). O PWA o exibe como `N01`, o mesmo nome usado no SD (`/BEE/N01/`).

### 5.2 Corpo da ingestão

```json
{
  "v": 1,
  "gateway_id": "GW01",
  "upload_id": 1842,
  "gw_time": 1790186460,
  "sessions": [
    {
      "node_id": 1,
      "boot_id": 7,
      "rx_time": 1790186421,
      "rssi": -71,
      "snr": 9,
      "attempts": [1, 1, 2],
      "status": "<base64 dos 36 B do STATUS>",
      "runs": [
        { "first_seq": 10234, "rec_ver": 1, "data": "<base64 de n × 24 B>" }
      ]
    }
  ]
}
```

- `runs` são trechos de `seq` contíguos; um backlog com lacunas vira vários trechos.
- `attempts` traz o campo `attempt` de cada frame da sessão, que alimenta o RNF-03.
- `upload_id` serve só para rastrear nos logs. A deduplicação é por `(node_id, seq)`, no banco.
- Um backlog de 36 frames (144 registros) ocupa \~4,6 KB em base64, bem abaixo do limite de 32 KB por POST.

### 5.3 Respostas

| Código | Quando | O que a central faz |
| --- | --- | --- |
| `200` | Tudo gravado ou já existente | Remove da fila; executa `commands`, se houver (Fase 2) |
| `400` | JSON malformado, tamanho de trecho que não é múltiplo de 24, `rec_ver` desconhecido | Quarentena (seção 4.2) |
| `401` | Token ausente ou inválido | Suspende o uplink e sinaliza no heartbeat |
| `413` | Corpo acima de 32 KB | Divide o lote |
| `429`, `5xx` | Throttling ou falha temporária | Nova tentativa com backoff |

```json
{ "upload_id": 1842, "stored": 6, "duplicates": 0, "conflicts": 0, "commands": [] }
```

`conflicts` conta registros com `seq` já existente e conteúdo diferente. Isso nunca deveria acontecer e indica falha no ring ou no `seq` (v1.2, seção 5.6); a Lambda mantém o original e registra o caso.

## 6. Modelo de dados (DynamoDB)

Duas tabelas bastam: uma para os registros, com chave `(node_id, seq)`, e uma para o estado atual de nós, centrais e diário.

### 6.1 `BeeCoolerReadings`

| Chave / índice | Atributos | Uso |
| --- | --- | --- |
| Tabela: PK `node_id` (N), SK `seq` (N) | — | Identidade do registro (v1.2, seções 5.1 e 6.7) |
| GSI `by_ts`: PK `node_id`, SK `ts` | projeção ALL | Séries por intervalo de tempo no dashboard |

Atributos de cada item:

- `ts` (epoch UTC usado nas consultas) e `ts_raw` (o valor que veio do nó);
- `t_in`, `rh_in`, `t_out`, `rh_out` em °C e %; `vbat` em V; `vib_rms`, `vib_peak` em mg; `band_db` (lista de 4, em dB);
- `flags` (16 bits, v1.2 seção 5.3) e `power_level` (bits 9–10, extraído para filtrar);
- `raw` (os 24 B originais, tipo Binary), `rec_ver`, `gateway_id`, `rx_time`, `ingested_at`.

**Sentinelas viram ausência.** Um campo com `INT16_MIN`, `UINT16_MAX` ou `0xFF` não é gravado; o gráfico mostra lacuna, e a flag correspondente explica o porquê (RF-01, RF-14).

**Por que `seq` e não `ts`.** Um GSI aceita chaves repetidas; a tabela, não. Com `SK = seq`, dois registros com o mesmo `ts` (relógio corrigido, `rtc_invalid`) continuam sendo dois itens. Quando um script corrige o `ts` de registros com `rtc_invalid` a partir do `seq` (UC-06), basta atualizar `ts`: o índice se ajusta e `ts_raw` preserva o original.

Um item tem \~300 B: \~45 KB/dia e \~16 MB/ano por nó.

### 6.2 `BeeCoolerState`

| PK | SK | Conteúdo | Escrito por |
| --- | --- | --- | --- |
| `NODE#<id>` | `STATUS` | Último STATUS decodificado, `max_seq`, último contato, RSSI/SNR, backlog, `power_level`, versão do firmware, histograma de `attempt` | Ingestão |
| `GW#<id>` | `HEARTBEAT` | Último heartbeat da central | Heartbeat |
| `JOURNAL` | `<inicio_utc>#<id>` | Entrada do diário: início, fim, tipo, descrição, responsável | `POST /v1/journal` |
| `NODE#<id>` | `CMD#<id>` | Comando pendente e seu estado (Fase 2) | `POST /commands` |

O último contato vem do `rx_time` da central, não da hora em que a nuvem recebeu o POST. Assim, uma queda do Wi-Fi não faz um nó saudável parecer atrasado depois que a fila é drenada.

### 6.3 Capacidade

Modo provisionado com 5 RCU e 5 WCU por tabela e por índice (15 de cada no total), sem auto scaling, cabe na oferta *always free* do DynamoDB (conferir na página de preços antes de criar). Com \~150 gravações por dia, o modo sob demanda também custaria frações de centavo.

## 7. Lambda

Uma única função (`BeeCoolerApi`, Python, 128–256 MB, timeout de 10 s, sem VPC) roteia todas as rotas. O decodificador do registro fica num módulo único do repositório, `beecooler_record.py`, usado pela Lambda, pelo coletor de bancada e pelos scripts de análise.

### 7.1 Decodificação do registro v1

```python
import struct

REC_V1 = struct.Struct('<IhHhHHHH4BH')   # 24 B, little-endian (v1.2, seção 5.2)
assert REC_V1.size == 24
I16_NA, U16_NA, U8_NA = -32768, 0xFFFF, 0xFF

def _v(x, na, k):
    return None if x == na else x * k

def decode_v1(buf: bytes, seq: int) -> dict:
    (ts, t_in, rh_in, t_out, rh_out, vbat, rms, peak,
     b0, b1, b2, b3, flags) = REC_V1.unpack(buf)
    return {
        'seq': seq, 'ts': ts, 'ts_raw': ts, 'flags': flags,
        'power_level': (flags >> 9) & 0b11,
        't_in':  _v(t_in,  I16_NA, 0.01), 'rh_in':  _v(rh_in,  U16_NA, 0.01),
        't_out': _v(t_out, I16_NA, 0.01), 'rh_out': _v(rh_out, U16_NA, 0.01),
        'vbat':  _v(vbat,  U16_NA, 0.001),
        'vib_rms': _v(rms, U16_NA, 0.1), 'vib_peak': _v(peak, U16_NA, 0.1),
        'band_db': [None if b == U8_NA else b / 2 - 100 for b in (b0, b1, b2, b3)],
        'raw': buf,
    }
```

O DynamoDB não aceita `float` do Python: os valores são convertidos para `Decimal` e os `None` são omitidos antes de gravar. O STATUS de 36 B tem um decodificador irmão no mesmo módulo.

### 7.2 Ingestão, passo a passo

1. Autenticar o token do gateway (seção 9).
2. Validar o JSON: tipos, `node_id` entre 1 e 254, tamanho de cada trecho múltiplo de 24, `rec_ver` conhecido, corpo ≤ 32 KB.
3. Decodificar cada registro e checar faixas físicas (−40 a 125 °C; 0 a 100% RH; 2,5 a 4,5 V). Fora da faixa, o campo é omitido e o item ganha `cloud_range_fail`; o registro não é descartado.
4. Gravar cada item com `PutItem` e a condição `attribute_not_exists(seq)`.
5. Se a condição falhar, comparar `raw`: igual conta como `duplicates`; diferente conta como `conflicts`, mantém o original e gera log de erro.
6. Atualizar `NODE#<id>/STATUS` só se o novo `rx_time` for mais recente que o gravado.
7. Responder `200` apenas depois que todas as gravações terminarem.

Com \~6 registros por sessão, gravações individuais são suficientes; `BatchWriteItem` não aceita condição e não é usado na ingestão.

### 7.3 Logs

Uma linha JSON por requisição: `gateway_id`, `upload_id`, `node_id`, `stored`, `duplicates`, `conflicts`, latência e erro. Tokens e corpos nunca vão para o log. Retenção do CloudWatch: 30 dias.

## 8. Dashboard PWA

O dashboard mostra tudo o que o RF-11 pede, a partir de duas fontes: a série `BeeCoolerReadings` e o estado `BeeCoolerState`. Ele é um build estático React + Vite + TypeScript no S3, servido pelo CloudFront.

### 8.1 Conteúdo

| Painel | Dados | Origem |
| --- | --- | --- |
| T/RH interna e externa | `t_in`, `rh_in`, `t_out`, `rh_out` com lacunas onde há sentinela | readings |
| Vibração | `vib_rms`, `vib_peak` e as 4 bandas (50–100, 100–200, 200–350, 350–600 Hz) | readings |
| Energia | `vbat`, `power_level`, flag `charging`, `vbat` sob carga do STATUS | readings + STATUS |
| Enlace | último contato, RSSI e SNR do ACK, backlog, % de frames na 1ª tentativa (RNF-03) | STATUS |
| Saúde do nó | flags de falha (SHT30, ADXL, SD, `rtc_invalid`, `sht_in_stuck`), espaço livre no SD, motivo do último reset, versão do firmware | readings + STATUS |
| Central | último heartbeat, fila, quarentena, Wi-Fi, NTP | HEARTBEAT |
| Diário | intervalos anotados como faixas sobre todos os gráficos | JOURNAL |

### 8.2 Estados do nó

A ordem de avaliação importa: primeiro a central, depois o nó.

| Estado | Condição | Exibição |
| --- | --- | --- |
| Central sem contato | Último heartbeat há mais de 30 min (2 perdidos) | Alerta da central; estado dos nós "desconhecido" |
| Nó silencioso | Central ok e último contato do nó há mais de 70 min (RF-11) | Alerta do nó |
| Nó em economia ou crítico | `power_level` ≥ 1 no último STATUS | Aviso de energia |
| Normal | Nenhum dos anteriores | — |

### 8.3 Atualização

A cada 60 s, o PWA consulta `GET /v1/nodes` (uma chamada). Ele só busca `readings` e `status` de um nó quando o `max_seq` ou o último contato mudam. Como os dados chegam uma vez por hora, a tela fica atualizada sem gerar tráfego à toa (D61).

### 8.4 Uso offline

O service worker guarda o app e a última resposta de cada rota. Sem rede ou com a API fora, o PWA mostra os dados guardados com o aviso "dados de HH:MM, sem atualização" e não quebra.

### 8.5 Diário de campo (D63)

Um formulário com os campos da seção 11 da v1.2: `inicio_utc`, `fim_utc`, `tipo`, `descricao`, `responsavel`. Os tipos são os mesmos: `inspecao`, `manutencao`, `rocagem`, `chuva_forte`, `alimentacao`, `troca_bateria`, `troca_sd`, `outro`. O botão "agora" preenche o início com a hora do celular, o que facilita anotar na hora da visita. O `GET /v1/export` inclui o diário, para que a limpeza do dataset use a mesma fonte.

## 9. Segurança

O risco principal não é o sigilo dos dados da colmeia, mas gravação forjada e custo por abuso das rotas públicas. As medidas abaixo cobrem os dois.

| Camada | Medida |
| --- | --- |
| Central | TLS validado com a raiz da Amazon Trust Services; NTP antes de abrir TLS; token próprio na NVS, nunca no repositório; nenhuma chave IAM no firmware |
| API — escrita | Token Bearer por gateway, mapeado `gateway_id → token`; comparação com `hmac.compare_digest`; o `gateway_id` do corpo precisa bater com o do token |
| API — leitura | Rotas públicas, sem dados pessoais; throttling no stage (10 req/s, rajada de 20) |
| API — equipe | Token de equipe para diário e comandos, guardado nas configurações do PWA no celular de cada integrante |
| Segredos | Protótipo: variável de ambiente da Lambda com o mapa de tokens. Fase 2: SSM Parameter Store (SecureString) |
| Lambda | Role com `PutItem`, `UpdateItem`, `GetItem` e `Query` só nas duas tabelas e no índice; sem VPC |
| CORS | Só a origem do CloudFront (`https://<distribuição>.cloudfront.net`), mais `http://localhost:5173` durante o desenvolvimento |
| S3 + CloudFront | Bucket privado, Origin Access Control, HTTPS obrigatório, fallback de SPA para `/index.html` |
| Conta | MFA na root; uso diário por usuário do IAM Identity Center; AWS Budget com alertas |

**Comandos remotos nunca contornam a segurança do atuador.** A duração máxima, o bloqueio por boia e por SLA baixa (RF-15, RF-16) e as regras de decisão (RF-17) ficam no C3 e no nó. Um comando vindo da nuvem é só uma sugestão de configuração, validada no nó antes de valer.

O HMAC com timestamp e anti-replay (v1.0, seção 8) fica registrado como evolução, para quando houver mais de uma central ou rede não confiável.

## 10. Custos e conta AWS

Na escala de um nó, o custo mensal é de centavos de dólar; o que pede atenção é o tipo de conta, não o volume.

### 10.1 Modelo de Free Tier vigente

A v1.0 descrevia o modelo antigo, com 12 meses gratuitos por serviço. A AWS mudou isso em 15/07/2025: clientes novos recebem até US$ 200 em créditos, e a oferta de 12 meses do API Gateway vale só para contas anteriores ([preços do API Gateway](https://aws.amazon.com/api-gateway/pricing/)). Contas novas escolhem entre plano Free e Paid; o Free dura 6 meses ou até acabarem os créditos e então a conta é fechada ([Rackspace, 2026](https://spot.rackspace.com/blog/aws-free-tier)).

Uma conta Free criada em outubro de 2026 fecharia por volta de abril de 2027, antes da Fase 3. **Recomendação:** plano Paid desde o início, consumindo os créditos, com AWS Budget em US$ 1 e US$ 5. Lambda e DynamoDB têm ofertas *always free*; API Gateway, S3 e CloudFront consomem créditos (conferir as páginas de preço antes da criação, P-AWS).

### 10.2 Tráfego estimado (um nó, uma central)

| Origem | Cálculo | Requisições/mês |
| --- | --- | --: |
| Ingestão | 24 sessões/dia × 30 | \~720 |
| Heartbeat | 96/dia × 30 | \~2.900 |
| PWA a cada 60 s, uma aba aberta 24 h | 1.440/dia × 30 | \~43.200 |
| PWA buscando séries quando há dado novo | \~48/dia × 30 | \~1.400 |
| **Total no pior caso** | — | **\~48.000** |
| Comparação: RF-11 original, a cada 2 s, uma aba 24 h | 43.200/dia × 30 × 3 rotas | \~3,9 milhões |

O pior caso fica abaixo de 5% de 1 milhão de requisições por mês. Armazenamento: \~16 MB por ano por nó (seção 6.1).

### 10.3 O que evita surpresa

- Throttling no stage da API (seção 9), porque o Budget só avisa e não bloqueia.
- Retenção de 30 dias nos logs do CloudWatch.
- Nada de VPC, NAT, Elastic IP, Provisioned Concurrency ou instância ligada.

## 11. Encaixe nas fases

A nuvem entra na Fase 1 e cresce por adição: nada do que é feito na Fase 1 é refeito depois. As fases seguem o Briefing v4.1 e a v1.2.

| Fase | Papel da nuvem | O que muda na integração |
| --- | --- | --- |
| 1 — bancada | Destino paralelo ao coletor serial | CT-03 continua com o coletor + SQLite (injeção de falhas na central); CT-11 e CT-12 validam a nuvem |
| 1 — campanha (tiúba, quintal) | Destino único da telemetria e dashboard da equipe | Central num carregador USB com o Wi-Fi da casa, sem notebook; o RAW de vibração continua só no SD, a nuvem recebe as features |
| 2 — MVP | Mesma pilha + EVENT imediato e comandos | `rec_ver = 2` decodificado pela Lambda; RF-19 encaminhado sem fila; UC-15 pela resposta do POST (D64) |
| 3 — apiário (Embrapa, \~300 m) | Sem mudança | Depende de Wi-Fi na sede (P-WIFI): portal cativo não funciona no ESP32; plano B é um roteador 4G |
| 4 — várias colmeias | Sem mudança de arquitetura | `node_id` e token por gateway já suportam N nós e N centrais; o SHT30 externo compartilhado entra como série da central |

O princípio da AP1 fica preservado em todas as fases: decisão e atuação acontecem no nó e no C3. Se a nuvem ou a internet caírem, a colmeia continua protegida e os dados esperam na fila da central.

## 12. Requisitos

O formato é o do Capítulo 2 da AP1 e da seção 13 da v1.2. Dois requisitos da v1.2 são reescritos e sete são novos; os demais não mudam.

### 12.1 Requisitos revisados

| ID | O sistema deve… | Critério de aceitação | Fase |
| --- | --- | --- | --- |
| RF-10 (rev.) | Registrar na nuvem os dados recebidos e os logs de comunicação | 100% dos registros aceitos pela central ficam no DynamoDB com chave `(node_id, seq)`; reenvios não criam itens; `GET /v1/export` gera CSV com registros e diário; em bancada, o coletor serial mantém o SQLite com `UNIQUE(node_id, seq)` | 1, 2 |
| RF-11 (rev.) | Mostrar no dashboard as séries de T/RH interna e externa, as features de vibração, a tensão da bateria e o estado do enlace | Painéis da seção 8.1; consulta a cada 60 s; registro de uma sessão visível em até 5 min após o fim dela; nó silencioso após 70 min sem contato; central sem contato após 30 min sem heartbeat, com os nós exibidos como "desconhecido" | 1, 2 |

O RNF-02 da v1.2 passa a ler "banco" como o DynamoDB.

### 12.2 Requisitos novos

| ID | O sistema deve… | Critério de aceitação | Fase |
| --- | --- | --- | --- |
| RF-20 | Encaminhar à nuvem tudo o que a central persistiu, sem perda nem duplicação, mesmo com o Wi-Fi fora | Com o Wi-Fi desligado por 12 h, todos os registros do período chegam ao DynamoDB em até 1 h após o retorno, sem lacunas de `seq` e com `duplicates` > 0 apenas nos reenvios; reiniciar a central durante um POST não perde nem duplica itens | 1, 2 |
| RF-21 | Na API, aceitar só gateways autenticados e payloads válidos | Token inválido → `401` sem gravação; payload malformado → `400` e o lote vai para a quarentena da central; sentinela vira campo ausente, nunca valor; `seq` repetido com conteúdo diferente → original mantido e `conflicts` > 0 | 1, 2 |
| RF-22 | Publicar a saúde da central | Heartbeat a cada 15 min com os campos da seção 4.3; o dashboard mostra fila, quarentena e NTP | 1, 2 |
| RF-23 | Registrar o diário de campo pelo dashboard | Entrada criada no celular aparece como faixa nos gráficos e no CSV exportado; sem o token da equipe, o POST é recusado | 1 |
| RF-24 | Entregar comandos remotos ao nó pela central | Comando criado no PWA sai na resposta do próximo POST, chega ao nó no ACK da sessão seguinte e seu estado (pendente, entregue, aplicado, recusado) aparece no dashboard; nenhum comando ultrapassa os limites do RF-15 a RF-17 | 2 |

| ID | Atributo | Valor alvo | Como se verifica |
| --- | --- | --- | --- |
| RNF-12 | Isolamento do enlace LoRa | Com o uplink ativo, o p99 do RTT fica dentro do timeout definido em P-RTT e nenhum ACK é perdido por bloqueio da central | CT-12 |
| RNF-13 | Custo operacional | ≤ US$ 1 por mês durante a campanha, no Cost Explorer | CT-07 |

## 13. Casos de teste

Os 8 testes da v1.0 foram absorvidos nos casos abaixo, no formato da seção 14 da v1.2. Todos rodam em bancada com `BEE_TEST_FAST` (sessão LoRa a cada 3 min), exceto o que depende da campanha. O CT-07 da v1.2 passa a usar o DynamoDB como banco e acrescenta o RNF-13 às evidências.

| ID | Requisitos | Objetivo | Ambiente | Evidências |
| --- | --- | --- | --- | --- |
| CT-11 | RF-10, RF-21 | Ingestão, validação, idempotência e exportação | bancada, `curl` + central | log da Lambda, export CSV |
| CT-12 | RF-20, RF-22, RNF-12 | Segundo salto com falhas e isolamento do enlace LoRa | bancada, modo de teste | log da central, DynamoDB, RTT do nó |
| CT-13 | RF-11, RF-23 | Dashboard, estados de alerta, offline e diário | bancada + celular | capturas de tela, export CSV |
| CT-14 | RF-24 | Comandos remotos | bancada | Fase 2 |

### CT-11 — Ingestão e API

**Procedimento**

1. Com `curl`, enviar um POST válido com 6 registros gerados pelo script de teste a partir de `beecooler_record.py`.
2. Reenviar o mesmo corpo 3 vezes.
3. Enviar um registro com o mesmo `seq` e um byte diferente.
4. Enviar: sem token; com token de outro gateway; JSON quebrado; trecho com 23 B; `rec_ver = 9`; corpo de 40 KB.
5. Enviar registros com cada sentinela (`INT16_MIN`, `UINT16_MAX`, `0xFF`) e um com `ts` repetido e `seq` diferente.
6. Exportar o CSV do intervalo e comparar com os bytes enviados.

**Resultado esperado**

- Passo 1: `200`, `stored = 6`. Passo 2: `stored = 0`, `duplicates = 6`, nenhum item novo.
- Passo 3: `conflicts = 1`, original mantido, log de erro.
- Passo 4: `401`, `401`, `400`, `400`, `400`, `413`, sem gravação.
- Passo 5: campos ausentes no item e no CSV; dois itens com o mesmo `ts`.
- Passo 6: CSV idêntico ao decodificado localmente.

### CT-12 — Segundo salto e isolamento do enlace

**Procedimento**

1. Rodar nó e central por 2 h com Wi-Fi e conferir a chegada de cada sessão.
2. Desligar o roteador por 12 h em modo de teste (240 sessões, \~1.440 registros) e religar.
3. Reiniciar a central no meio de um POST, 5 vezes.
4. Bloquear a resposta HTTP na central (opção de teste: descartar o `200` recebido) em 10 POSTs.
5. Durante 500 trocas LoRa com o uplink em andamento, registrar o RTT no nó (mesma medição de P-RTT).
6. Configurar um token errado na central e observar o heartbeat.

**Resultado esperado**

- Todos os `seq` no DynamoDB, sem lacunas; fila zerada em até 1 h após o retorno do Wi-Fi.
- Reenvios aparecem como `duplicates`, nunca como itens novos.
- p99 do RTT com uplink ≤ timeout de P-RTT; nenhuma sessão abortada por bloqueio da central.
- Com token errado: uplink suspenso, fila preservada, alerta no heartbeat seguinte.

### CT-13 — Dashboard e diário

**Procedimento**

1. Com dados do CT-12, abrir o PWA no notebook e no celular e instalar no celular.
2. Desconectar um SHT30 por 2 ciclos e conferir a lacuna e a flag no gráfico.
3. Desligar o nó por 75 min com a central ligada.
4. Desligar a central por 35 min.
5. Com o celular em modo avião, abrir o PWA.
6. Criar uma entrada do diário pelo celular; tentar criar outra sem o token da equipe.
7. Medir, em 10 sessões, o tempo entre o último ACK e a aparição do registro na tela.

**Resultado esperado**

- Todos os painéis da seção 8.1 preenchidos; lacuna, não valor, onde houve falha.
- Passo 3: "nó silencioso". Passo 4: "central sem contato" e nós "desconhecido", sem alerta de nó.
- Passo 5: últimos dados com o aviso de horário, sem tela de erro.
- Passo 6: faixa no gráfico e linha no CSV; segunda tentativa recusada.
- Passo 7: ≤ 5 min em todas as sessões.

## 14. Rastreabilidade

Cada requisito novo ou revisado tem um ponto de atendimento neste documento e um caso de teste; os demais seguem a seção 15 da v1.2.

| Requisito | Onde é atendido | Caso de teste | Situação |
| --- | --- | --- | --- |
| RF-10 (rev.) | 5.1 (`/export`), 6.1, 7.2 | CT-11, CT-07 | A verificar |
| RF-11 (rev.) | 8.1 a 8.4 | CT-13, CT-07 | A verificar |
| RF-20 | 4.1, 4.2 | CT-12 | A verificar |
| RF-21 | 5.3, 7.2, 9 | CT-11 | A verificar |
| RF-22 | 4.3, 6.2, 8.2 | CT-12, CT-13 | A verificar |
| RF-23 | 5.1, 6.2, 8.5 | CT-13 | A verificar |
| RF-24 | 5.1, 5.3, 9 | CT-14 | Fase 2 |
| RNF-02 (leitura nova) | 6.1 | CT-07 | A verificar |
| RNF-12 | 4.1 | CT-12 | A verificar |
| RNF-13 | 10 | CT-07 | A verificar |

### 14.1 O que mudou em relação à v1.0

| Ponto da v1.0 | Problema | Tratamento nesta versão |
| --- | --- | --- |
| Chave `node_id` + `timestamp` | `ts` repetido sobrescreve dados sem erro | D56, seção 6.1 |
| JSON com floats e `pump_state` | Sem `seq`, bandas, pico e sentinelas; campo da Fase 2 | D57, seções 5.2 e 7.1 |
| Lote a cada 30 min | Cadência inexistente no projeto; falso "nó silencioso" | D58, seção 4 |
| Loop único no gateway, sem ACK | POST bloqueante atrasa o ACK LoRa | D59, seção 4.1 |
| `/status` opcional | RF-11 exige enlace, backlog e energia | Seções 6.2 e 8.1 |
| 10 mil GETs/mês | RF-11 a cada 2 s gera milhões | D61, seção 10.2 |
| Free Tier de 12 meses | Não vale para contas novas | Seção 10.1 |
| "Fases 1 a 7" e "MVP" | Colidem com as Fases 0 a 4 e com o MVP da Fase 2 | "Etapas" (seção 16) e critério de conclusão da integração |
| Token único comparado com `==` | Contradiz token por gateway; comparação não constante | D62, seção 9 |
| Sem menção ao IoT Core | A v1.2 cita MQTT para a Fase 2 | D60, seção 17 |

## 15. Pendências

Seis pendências novas, no formato da seção 12 da v1.2.

| ID | O que é | Por que importa | Como medir ou decidir | O que decide |
| --- | --- | --- | --- | --- |
| **P-TLS** | Heap e duração do handshake TLS no WROOM com LoRa, LittleFS e Wi-Fi ativos | Falta de heap derruba o uplink ou a central; handshake longo disputa CPU com o rádio | `esp_get_free_heap_size()` antes e durante 100 POSTs; tempo do handshake por `esp_timer` | Reuso de conexão, tamanho dos buffers do mbedTLS |
| **P-LFS** | Tamanho da partição LittleFS da central | A fila precisa aguentar quedas longas de Wi-Fi | Tabela de partições do WROOM (4 MB) | Partição de 1 MB como alvo (\~150 dias de um nó) |
| **P-WIFI** | Rede disponível na sede da Embrapa (Fase 3) | Portal cativo não funciona no ESP32; WPA2-Enterprise exige configuração própria | Visita técnica e conversa com a TI local | Wi-Fi local ou roteador 4G |
| **P-AWS** | Plano da conta (Free × Paid), região e limites vigentes | O plano Free fecha em 6 meses | Conferir as páginas de preço no dia da criação | Plano, região (`us-east-1` ou `sa-east-1`) |
| **P-POLL** | Cadência de consulta do PWA | 60 s é proposta; a equipe pode querer menos tráfego ou mais rapidez | Uso real durante o CT-13 | Valor final no RF-11 |
| **P-ROOT** | Certificado raiz a gravar na central | Troca de cadeia da AWS invalida o pin | Conferir a cadeia do endpoint do API Gateway | Raiz da Amazon Trust Services + procedimento de atualização por OTA ou USB |

## 16. Etapas de implementação

As etapas são incrementais e cada uma termina com algo testável. Usamos "etapa" para não confundir com as Fases 0 a 4 do projeto.

1. **Conta.** Plano Paid com créditos, MFA na root, usuário do IAM Identity Center, Budget em US$ 1 e US$ 5, região escolhida (P-AWS).
2. **Parser compartilhado.** `beecooler_record.py` com `decode_v1` e o decodificador do STATUS, testado contra registros gerados pelo firmware em bancada.
3. **Banco.** Tabelas `BeeCoolerReadings` (com o GSI `by_ts`) e `BeeCoolerState`.
4. **Lambda e API.** Role mínima, função com as rotas de ingestão e heartbeat, HTTP API, throttling, CORS. Executar o CT-11 com `curl`.
5. **Central.** `task_uplink`, fila no LittleFS, NTP, TLS com a raiz gravada, heartbeat. Medir P-TLS e executar o CT-12.
6. **Rotas de leitura e export.** `nodes`, `status`, `readings`, `gateways`, `export`.
7. **PWA.** Painéis da seção 8.1, estados da seção 8.2, service worker, diário. Build no S3 privado, CloudFront com OAC e fallback de SPA. Executar o CT-13.
8. **Campanha.** CT-07 com a nuvem como destino; conferir o RNF-13 no Cost Explorer ao fim.

### 16.1 Critério de conclusão da integração

- [ ] CT-11, CT-12 e CT-13 executados, com evidências em `ensaios/ct11/`, `ensaios/ct12/` e `ensaios/ct13/`
- [ ] Nenhuma credencial no repositório; tokens só na NVS da central e na configuração da Lambda
- [ ] Budget ativo e retenção de logs em 30 dias
- [ ] Seção 7.3 da Arquitetura v1.2 atualizada com D54 e D55
- [ ] RF-10 e RF-11 atualizados na seção 13 da Arquitetura e na rastreabilidade (seção 15)

## 17. Alternativas descartadas

Cada uma resolve um problema que o BeeCooler ainda não tem; nenhuma é descartada por ser ruim.

| Alternativa | Por que não agora | Quando reconsiderar |
| --- | --- | --- |
| AWS IoT Core (MQTT) | Exige certificado X.509 por dispositivo e conexão MQTT/TLS persistente na central; o tráfego é um POST por hora, e o downlink da Fase 2 cabe na resposta do POST (D64) | Comandos com latência de segundos ou dezenas de centrais |
| EC2 + servidor próprio | Máquina ligada 24 h, manutenção de SO, custo fixo | Processamento contínuo pesado na nuvem |
| RDS / PostgreSQL | Instância permanente, conexões, backups; o padrão de acesso é simples e previsível | Consultas analíticas complexas sobre muitas colmeias |
| Amazon Timestream | Mais um serviço para um volume de \~16 MB/ano | Milhões de pontos por dia |
| Coletor serial + SQLite na campanha | Notebook ligado 15 dias na casa; dashboard só local | Continua como ferramenta de bancada (D55) |
| Lote fixo de 30 min | Desalinha das sessões horárias e atrasa o alerta de nó silencioso | — |

## 18. Referências

1. BeeCooler — AP1: Relatório do Ciclo de Projeto 1 (Fase 0), 21/09/2026 — seções 3.1.2, 7.1 e 7.2.
2. BeeCooler — Arquitetura de Software do Nó Sensor e da Telemetria v1.2, 02/10/2026 — seções 5, 6, 7, 11, 13, 14 e 15.
3. BeeCooler — Briefing de Decisões v4.1 e `hardware.md` (fases do projeto).
4. BeeCooler — Dashboard IoT Serverless na AWS v1.0 (versão substituída).
5. AWS — [Amazon API Gateway pricing](https://aws.amazon.com/api-gateway/pricing/), consultado em 02/10/2026.
6. Rackspace — [AWS Free Tier Explained: What's Actually Free in 2026](https://spot.rackspace.com/blog/aws-free-tier), consultado em 02/10/2026.
7. AWS — páginas de preço de [Lambda](https://aws.amazon.com/lambda/pricing/), [DynamoDB](https://aws.amazon.com/dynamodb/pricing/), [S3](https://aws.amazon.com/s3/pricing/) e [CloudFront](https://docs.aws.amazon.com/AmazonCloudFront/latest/DeveloperGuide/flat-rate-pricing-plan.html), a conferir no dia da criação da conta (P-AWS).
8. AWS — [ciclo de vida do ambiente de execução da Lambda](https://docs.aws.amazon.com/lambda/latest/dg/lambda-runtime-environment.html), [IAM](https://docs.aws.amazon.com/IAM/latest/UserGuide/introduction.html) e [Budgets](https://docs.aws.amazon.com/cost-management/latest/userguide/budgets-managing-costs.html).
