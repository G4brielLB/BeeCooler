"""Parâmetros do pipeline de TinyML do BeeCooler.

Tudo que é escolha (limiares, horizontes, regras de QC) fica aqui, para que o
pipeline possa ser re-treinado com novas coletas sem mexer no código: basta
colocar os novos dias em `coleta_dados/<campanha>/BEE/<nó>/<AAAAMMDD>/` e rodar
`python run_all.py`.

Convenção dos documentos: ✅ fechado · 🟡 provisório · 🔴 pendente de validação.
"""
import os
from pathlib import Path

ML_DIR = Path(__file__).resolve().parent
# BeeCooler/analysis/ml -> system-computationals/
ROOT = ML_DIR.parents[2]
# Raiz dos dados de campo (somente leitura, fora do repositório). Cada subpasta com
# BEE/<nó>/<dia>/rec.bin é lida. Pode ser trocada pela variável de ambiente BEECOOLER_DATA.
DATA_ROOT = Path(os.environ.get("BEECOOLER_DATA", ROOT / "coleta_dados"))
OUT = ML_DIR / "out"

# Fuso do local de coleta (o DS1302 grava epoch UTC).
UTC_OFFSET_H = -3

# ---------------------------------------------------------------- QC
QC = dict(
    nominal_period_s=600,          # grade de 10 min
    test_fast_period_s=60,         # registros com período <= isto são do modo TEST_FAST (bancada)
    segment_gap_s=3600,            # lacuna > 60 min quebra o segmento (o nó também perde a RTC RAM num cold boot)
    t_valid=(5.0, 60.0),           # faixa física plausível de T (°C)
    rh_out_min=0.5,                # RH_ext <= isto = falha do SHT30 externo (observado em 24/11 e 27/11)
    max_step_c=10.0,               # salto > 10 °C em 10 min = suspeito (sol direto já dá +6 °C em 10 min no sensor externo)
    vib_peak_sat=6553.0,           # vib_peak saturado (0xFFFF·0,1 mg)
)

# ---------------------------------------------------------------- features
# Constantes de tempo das médias móveis exponenciais (inércia térmica), em minutos.
EMA_TAUS_MIN = (30, 60, 120, 240)
# Amostras descartadas no início de cada segmento (EMA ainda não convergiu): 60 min.
WARMUP_MIN = 60

# ---------------------------------------------------------------- rótulos (🟡 provisórios)
# Não há limiar publicado específico para tiúba (Melipona fasciculata); ver
# docs/tinyml_pipeline_v0.md, seção "Revisão rápida". Valores escolhidos como
# ponto de partida e avaliados em análise de sensibilidade.
LABEL = dict(
    t_on=36.0,             # liga a nebulização quando T_in >= t_on (ou a previsão atinge t_on)
    t_off=35.0,            # desliga quando T_in <= t_off (histerese de 1 °C)
    min_on_min=20,         # tempo mínimo ligado
    min_off_min=20,        # tempo mínimo desligado
    t_alarm=40.0,          # alarme (aproxima-se de CTmax de outros Meliponini, ~43–44 °C)
    day_window_h=(7, 18),  # TinyML só roda de dia (D34)
)
SENSITIVITY_T_ON = (35.0, 36.0, 37.0)

# ---------------------------------------------------------------- modelos
HORIZON_MIN = 30            # previsão de T_in em +30 min (3 passos de 10 min)
RANDOM_STATE = 0
MLP_HIDDEN = (16, 8)
# L2 forte: com ~650 amostras a MLP sobreajusta com alpha pequeno (varredura em
# docs/tinyml_pipeline_v0.md: alpha 1e-3/1e-1/1 × camadas (4,)/(8,)/(16,8)).
MLP_ALPHA = 1.0
# Modelo exportado por tarefa: "auto" = MLP só se o MAE LODO for >= MIN_GAIN melhor que o
# linear (o mais simples vence empate); ou force "linear" / "mlp".
MODEL_CHOICE = {"FC": "auto", "VS": "auto"}
MLP_MIN_GAIN = 0.05
