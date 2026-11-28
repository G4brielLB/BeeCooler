# Pipeline de TinyML — BeeCooler

> Dados de campo (`coleta_dados/`, fora do repositório, somente leitura) → QC → features → rótulos → modelos (validação deixando um dia de fora) → cabeçalho C para o ESP32-S3.
> Decisões, resultados e limites: **[`docs/tinyml_pipeline_v0.md`](../../docs/tinyml_pipeline_v0.md)**.

## Rodar

O ambiente Python é o da raiz do repositório (`BeeCooler/.venv`, dependências em `BeeCooler/requirements.txt`):

```powershell
cd BeeCooler
uv venv .venv --python 3.12
uv pip install --python .venv/Scripts/python.exe -r requirements.txt
cd analysis/ml
../../.venv/Scripts/python.exe run_all.py
```

Por padrão os dados são lidos de `../coleta_dados` em relação à pasta que contém o repositório; em outra máquina, aponte `BEECOOLER_DATA` para a pasta da coleta (`$env:BEECOOLER_DATA = "D:/coleta_dados"`).

`requirements.lock.txt` congela as versões com que a v0 do pipeline foi treinada. O teste de paridade C precisa de um `gcc` no PATH; sem ele, o resto roda e a paridade é marcada como "não executado".

## Re-treinar com mais dados

1. Copie a nova coleta para `coleta_dados/<campanha>/BEE/<nó>/<AAAAMMDD>/` (com `rec.bin`), sem alterar os originais.
2. Rode `run_all.py`. Todas as pastas com `rec.bin` são lidas, o QC e a validação deixando um dia de fora são refeitos, e o modelo exportado é escolhido de novo (`config.MODEL_CHOICE = "auto"`: a MLP só é exportada se for ≥ 5 % melhor que o linear).
3. Compare `out/metricas.json` com a versão anterior antes de levar o novo `out/c/beecooler_tinyml.h` ao firmware.

Limiares, horizonte, regras de QC e hiperparâmetros ficam em `config.py`.

## Estrutura

| Arquivo | Papel |
|---|---|
| `config.py` | caminhos, regras de QC, limiares de rótulo (🟡), horizonte, hiperparâmetros |
| `beeml/ingest.py` | decodifica `rec.bin` (ExtendedRecord 64 B, CRC16) |
| `beeml/qc.py` | marca exclusões (TEST_FAST, falha do SHT30 externo, saltos) e segmentos contínuos |
| `beeml/features.py` | features em *streaming* (EMAs com passo irregular, tendências, hora) — mesmo algoritmo do C |
| `beeml/labels.py` | rótulo por histerese, rótulo antecipado, episódios |
| `beeml/train.py` | baselines + linear + árvore + MLP; validação deixando um dia de fora; avaliação da decisão da bomba |
| `beeml/export.py` | modelo final → `out/c/beecooler_tinyml.h` (float32 e INT8), vetores de teste, paridade C |
| `beeml/eda.py` | figuras |
| `run_all.py` | executa tudo e grava `out/` |

## Saídas (`out/`)

`registros_qc.csv` (todos os registros com motivo de exclusão) · `features_rotulos.csv` · `episodios_regra.csv` · `pred_lodo_*.csv` (previsões fora da amostra) · `metricas.json` · `run_log.txt` · `figs/*.png` · `c/beecooler_tinyml.h`, `c/test_vectors.csv`, `c/parity_test.c`.
