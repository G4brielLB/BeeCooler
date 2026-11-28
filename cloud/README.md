# Nuvem BeeCooler (AWS, us-east-2)

| Pasta | Lambda | Rotas |
|---|---|---|
| `ingest/` | ingestão (já existe na AWS) | `POST /v1/telemetry`, `POST /v1/gateway/heartbeat` |
| `read_api/` | leitura (nova) | `GET /v1/nodes`, `GET /v1/nodes/{node_id}/readings`, `GET /v1/nodes/{node_id}/status`, `GET /v1/gateways/{gateway_id}/status` |

As duas usam as tabelas `BeeCoolerReadings` (PK `node_id` N, SK `seq` N) e `BeeCoolerState` (PK `PK` S, SK `SK` S).
Formato das respostas GET: `dashboard/README.md`.

## 1. Atualizar a Lambda de ingestão

Envie `ingest/lambda_function.py` e `ingest/record.py` (com `decode_status`). Mudanças em relação à versão anterior:

- grava `NODE#<id>/STATUS` a cada sessão: `max_seq`, `last_contact` (só avançam), STATUS de 36 B decodificado
  (backlog, SD, motivo do reset, RSSI do ACK, sessões falhas, bateria sob carga, firmware), `power_level`
  e o histograma de tentativas (`att_1`…`att_4`, só de registros novos);
- heartbeat com `received_at` (hora da nuvem) e com os nomes que o gateway envia (`uptime_s`, `heap_free`,
  `queue`, `quarantine`, `fw`); antes `uptime`, `free_heap` e `queue_len` chegavam sempre vazios;
- `400` para trecho que não é múltiplo de 24 B, base64 inválido ou `rec_ver` desconhecido (Nuvem 5.3).

## 2. Índice `by_ts` em `BeeCoolerReadings`

DynamoDB → `BeeCoolerReadings` → Índices → Criar índice: chave de partição `node_id` (Número),
chave de classificação `ts` (Número), nome `by_ts`, projeção "Todos os atributos".
Sem ele a leitura ainda funciona, mas lê todo o histórico do nó a cada consulta.

## 3. Criar a Lambda de leitura

1. Lambda → Criar função `BeeCoolerReadApi`, Python 3.12; cole `read_api/lambda_function.py`.
2. Variáveis de ambiente (opcionais; os padrões já batem com as tabelas atuais):
   `READINGS_TABLE`, `STATE_TABLE`, `TS_INDEX=by_ts`, `ALLOWED_ORIGIN` (ex.: a URL onde o painel for publicado; padrão `*`).
3. Permissões da função: acrescente ao papel de execução somente leitura:

```json
{
  "Version": "2012-10-17",
  "Statement": [{
    "Effect": "Allow",
    "Action": ["dynamodb:Query", "dynamodb:GetItem", "dynamodb:Scan"],
    "Resource": [
      "arn:aws:dynamodb:us-east-2:<conta>:table/BeeCoolerReadings",
      "arn:aws:dynamodb:us-east-2:<conta>:table/BeeCoolerReadings/index/by_ts",
      "arn:aws:dynamodb:us-east-2:<conta>:table/BeeCoolerState"
    ]
  }]
}
```

## 4. Rotas no API Gateway (`BeeCoolerHttpApi`)

1. Rotas → Criar, uma para cada rota GET da tabela acima (com `{node_id}` e `{gateway_id}` literalmente).
2. Integrações → anexe as quatro à `BeeCoolerReadApi`.
3. CORS → Access-Control-Allow-Origin: a origem do painel (ou `*`); métodos `GET`; cabeçalhos `content-type`.
4. Se o stage não tiver implantação automática, implante.
5. Throttling do stage (Nuvem D61): por exemplo, 10 req/s com burst 20.

Teste: `curl -s https://<id>.execute-api.us-east-2.amazonaws.com/v1/nodes`

## Testes locais

```bash
pip install boto3 "moto[dynamodb]"
python -m unittest discover -s cloud/tests -v
```
