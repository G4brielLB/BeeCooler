# Dashboard BeeCooler

Painel estático (HTML, CSS e JavaScript puro, sem build) que lê as rotas GET da API em nuvem
(`docs/BeeCooler — Integração em Nuvem (AWS Serverless) v1.1.md`, seção 5.1). Visual e regras de período: `BeeCooler_Dashboard_v5 (2).md`.

## Rodar localmente

```bash
cd dashboard
python3 -m http.server 8000
```

- `http://localhost:8000/?mock`: demonstração com os 49 registros do teste LoRa de 05/10/2026, sem API.
- `http://localhost:8000/`: dados reais; antes, preencha `API_BASE` em `js/config.js`.
- `#sistema` no fim da URL abre direto a aba Sistema.

## Arquivos

| Arquivo | Função |
|---|---|
| `index.html` | Marcação e CSS do protótipo |
| `js/config.js` | URL da API, nó, intervalos de alerta e faixas ideais |
| `js/api.js` | Chamadas GET, paginação e normalização das respostas |
| `js/app.js` | Períodos, agregação, gráficos, alertas e atualização a cada 60 s |
| `js/mock.js` | Respostas simuladas para `?mock` |

## O que a API precisa ter

| Rota | Usada para |
|---|---|
| `GET /v1/nodes` | Lista de nós com `node_id`, `max_seq`, `last_contact`, `gateway_id` (consultada a cada 60 s) |
| `GET /v1/nodes/{id}/readings?from=&to=&next=` | Leituras por intervalo de `ts` (epoch s), em `{ items, next }` |
| `GET /v1/nodes/{id}/status` | Último STATUS: `last_contact`, `rssi`, `snr`, `backlog`, `power_level`, `attempt_hist`, `sd_free_mb`, `sd_errors`, `reset_reason`, `failed_sessions`, `vbat_load`, `fw_version` |
| `GET /v1/gateways/{id}/status` | Último heartbeat: `received_at`, `uptime_s`, `free_heap`, `wifi_rssi`, `queue`, `quarantine`, `ntp_ok`, `fw_version` |

As leituras usam os nomes do DynamoDB (`t_in`, `rh_in`, `t_out`, `rh_out`, `vib_rms`, `vib_peak`, `band_db`, `vbat`, `seq`, `flags`).
Campos ausentes viram lacunas nos gráficos. Se a Lambda usar outros nomes, ajuste só `js/api.js`.

O API Gateway precisa liberar CORS (`GET`) para a origem onde o painel for publicado.
