"""Treina os modelos finais com todos os dados e gera um cabeçalho C para o ESP32-S3.

Saídas:
  lib/BeeCoolerML/src/beecooler_tinyml.h  pesos (float32 e INT8), estado das features
                          (RTC RAM), inferência e camada de decisão (histerese) —
                          C99 puro, sem dependência de biblioteca;
  test/test_tinyml/vectors.h  vetores do teste nativo do PlatformIO;
  out/c/test_vectors.csv  sequência de entradas → saídas esperadas (Python);
  out/c/parity_test.c     confere o C contra o Python (gcc no host).

Modelo por tarefa: linear (Ridge, 1 camada) ou MLP, escolhido pela validação LODO
(config.MODEL_CHOICE). Os dois usam o mesmo motor em C: a entrada é padronizada
((x − μ)/σ, constantes exportadas) e passa por camadas densas com ReLU entre elas.
INT8: quantização simétrica por camada dos pesos sobre a entrada padronizada
(ativações em float), suficiente para um modelo deste tamanho.
"""
from __future__ import annotations

import subprocess
from pathlib import Path

import numpy as np
import pandas as pd

from .features import FC_FEATURES, VS_FEATURES, FeatState
from .labels import hysteresis
from .train import linear, mlp


def _extract(pipe):
    """(μ, σ, [W], [b]) de um Pipeline(StandardScaler, Ridge|MLPRegressor)."""
    sc, m = pipe.steps[0][1], pipe.steps[1][1]
    if hasattr(m, "coefs_"):
        W, b = [w.copy() for w in m.coefs_], [x.copy() for x in m.intercepts_]
    else:
        W, b = [np.asarray(m.coef_, dtype=float).reshape(-1, 1)], [np.atleast_1d(float(m.intercept_))]
    return sc.mean_.copy(), sc.scale_.copy(), W, b


def _forward(net, x, W=None):
    mu, sd, W0, b = net
    W = W0 if W is None else W
    h = (np.asarray(x, dtype=np.float64) - mu) / sd
    for i in range(len(W)):
        h = h @ W[i] + b[i]
        if i < len(W) - 1:
            h = np.maximum(h, 0)
    return h[..., 0]


def _quant(W):
    q, s = [], []
    for w in W:
        sc = np.max(np.abs(w)) / 127.0
        q.append(np.clip(np.round(w / sc), -127, 127).astype(np.int8))
        s.append(sc)
    return q, s


def _c_arr(name, a, ctype):
    flat = a.flatten(order="C")  # W[i] tem forma (entradas, saídas): índice [in*n_out + out]
    if ctype == "float":
        body = ", ".join(f"{v:.8e}f" for v in flat)
    else:
        body = ", ".join(str(int(v)) for v in flat)
    return f"static const {ctype} {name}[{len(flat)}] = {{{body}}};\n"


def _emit_net(prefix, net, feats, kind):
    mu, sd, W, b = net
    q, s = _quant(W)
    dims = [W[0].shape[0]] + [w.shape[1] for w in W]
    s_ = (f"/* {prefix}: {kind} {'-'.join(map(str, dims))}, ReLU entre camadas, saída linear.\n"
          f"   Entradas (padronizadas com {prefix}_MU/{prefix}_SD): {', '.join(feats)} */\n")
    s_ += f"#define {prefix}_N_IN {dims[0]}\n#define {prefix}_N_LAYERS {len(W)}\n"
    s_ += _c_arr(f"{prefix}_MU", mu.astype(np.float32), "float") + _c_arr(f"{prefix}_SD", sd.astype(np.float32), "float")
    s_ += f"static const int {prefix}_DIMS[{len(dims)}] = {{{', '.join(map(str, dims))}}};\n"
    for i in range(len(W)):
        s_ += _c_arr(f"{prefix}_W{i}", W[i].astype(np.float32), "float")
        s_ += _c_arr(f"{prefix}_W{i}_Q", q[i], "int8_t")
        s_ += _c_arr(f"{prefix}_B{i}", b[i].astype(np.float32), "float")
    s_ += f"static const float *const {prefix}_W[{len(W)}] = {{{', '.join(f'{prefix}_W{i}' for i in range(len(W)))}}};\n"
    s_ += f"static const int8_t *const {prefix}_WQ[{len(W)}] = {{{', '.join(f'{prefix}_W{i}_Q' for i in range(len(W)))}}};\n"
    s_ += f"static const float {prefix}_WS[{len(W)}] = {{{', '.join(f'{v:.8e}f' for v in s)}}};\n"
    s_ += f"static const float *const {prefix}_B[{len(W)}] = {{{', '.join(f'{prefix}_B{i}' for i in range(len(W)))}}};\n\n"
    return s_, q, s


HEADER_TAIL = r"""
/* ------------------------------------------------------------------ inferência */
#define BCML_MAX_W 32
static float bcml_mlp(const float *x, const float *mu, const float *sd, int n_layers, const int *dims,
                      const float *const *W, const int8_t *const *WQ, const float *WS, const float *const *B,
                      int use_int8) {
  float a[BCML_MAX_W], o[BCML_MAX_W];
  int l, i, j;
  for (i = 0; i < dims[0]; ++i) a[i] = (x[i] - mu[i]) / sd[i];
  for (l = 0; l < n_layers; ++l) {
    int n_in = dims[l], n_out = dims[l + 1];
    for (j = 0; j < n_out; ++j) {
      float acc = 0.0f;
      for (i = 0; i < n_in; ++i)
        acc += a[i] * (use_int8 ? (float)WQ[l][i * n_out + j] * WS[l] : W[l][i * n_out + j]);
      acc += B[l][j];
      o[j] = (l < n_layers - 1 && acc < 0.0f) ? 0.0f : acc;
    }
    for (j = 0; j < n_out; ++j) a[j] = o[j];
  }
  return a[0];
}

/* ------------------------------------------------------------------ estado (RTC RAM) */
typedef struct {
  uint32_t ts;          /* 0 = sem estado (reset/cold boot) */
  float ema_out[BCML_N_TAUS];
  float ema_rh;
  float tin[4], tout[4];
  uint32_t tsh[4];
  uint8_t n;            /* amostras no histórico (até 4) */
  uint32_t age_s;
  /* camada de decisão */
  uint8_t pump_on;
  uint32_t pump_since;
} bcml_state_t;

static void bcml_reset(bcml_state_t *s) { memset(s, 0, sizeof(*s)); }

static float bcml_slope(const bcml_state_t *s, const float *h, int k) {
  int last = s->n - 1, j;
  float dt;
  if (s->n < 2) return 0.0f;
  j = last - k; if (j < 0) j = 0;
  dt = (float)(s->tsh[last] - s->tsh[j]);
  return dt > 0.0f ? (h[last] - h[j]) / (dt / 600.0f) : 0.0f;
}

static void bcml_push(float *h, float v, uint8_t n) {
  int i;
  if (n < 4) { h[n] = v; return; }
  for (i = 0; i < 3; ++i) h[i] = h[i + 1];
  h[3] = v;
}

/* Atualiza o estado com um registro novo e preenche as features nomeadas. */
typedef struct { float t_in, rh_in, t_out, rh_out, ema_rh_out_60, d_tin_10, d_tin_30, d_tout_30, h_sin, h_cos, age_min;
                 float ema_out[BCML_N_TAUS]; } bcml_feat_t;

static void bcml_update(bcml_state_t *s, uint32_t ts, float t_in, float rh_in, float t_out, float rh_out,
                        float hour_local, bcml_feat_t *f) {
  int k;
  if (s->ts == 0) {
    for (k = 0; k < BCML_N_TAUS; ++k) s->ema_out[k] = t_out;
    s->ema_rh = rh_out;
  } else {
    float dt = (float)(ts - s->ts);
    s->age_s += (uint32_t)dt;
    for (k = 0; k < BCML_N_TAUS; ++k) s->ema_out[k] += (1.0f - expf(-dt / (BCML_TAUS_MIN[k] * 60.0f))) * (t_out - s->ema_out[k]);
    s->ema_rh += (1.0f - expf(-dt / 3600.0f)) * (rh_out - s->ema_rh);
  }
  s->ts = ts;
  bcml_push(s->tin, t_in, s->n); bcml_push(s->tout, t_out, s->n);
  { uint32_t *h = s->tsh; if (s->n < 4) h[s->n] = ts; else { h[0] = h[1]; h[1] = h[2]; h[2] = h[3]; h[3] = ts; } }
  if (s->n < 4) s->n++;
  f->t_in = t_in; f->rh_in = rh_in; f->t_out = t_out; f->rh_out = rh_out; f->ema_rh_out_60 = s->ema_rh;
  for (k = 0; k < BCML_N_TAUS; ++k) f->ema_out[k] = s->ema_out[k];
  f->d_tin_10 = bcml_slope(s, s->tin, 1); f->d_tin_30 = bcml_slope(s, s->tin, 3); f->d_tout_30 = bcml_slope(s, s->tout, 3);
  f->h_sin = sinf(2.0f * 3.14159265f * hour_local / 24.0f); f->h_cos = cosf(2.0f * 3.14159265f * hour_local / 24.0f);
  f->age_min = s->age_s / 60.0f;
}

/* Previsão de T_in em +BCML_HORIZON_MIN (°C). */
static float bcml_forecast(const bcml_feat_t *f, int use_int8) {
  float x[FC_N_IN] = {f->t_in, f->d_tin_10, f->d_tin_30, f->rh_in, f->t_out, f->d_tout_30,
                      f->ema_out[BCML_IDX_EMA60], f->ema_out[BCML_IDX_EMA240], f->h_sin, f->h_cos};
  return f->t_in + bcml_mlp(x, FC_MU, FC_SD, FC_N_LAYERS, FC_DIMS, FC_W, FC_WQ, FC_WS, FC_B, use_int8);
}

/* Sensor virtual: T_in estimada só com o sensor externo (°C). */
static float bcml_virtual_tin(const bcml_feat_t *f, int use_int8) {
  float x[VS_N_IN] = {f->t_out, f->rh_out, f->ema_out[0], f->ema_out[1], f->ema_out[2], f->ema_out[3], f->ema_rh_out_60,
                      f->h_sin, f->h_cos};
  return bcml_mlp(x, VS_MU, VS_SD, VS_N_LAYERS, VS_DIMS, VS_W, VS_WQ, VS_WS, VS_B, use_int8);
}

/* Camada de decisão (regras de segurança). Devolve 1 = nebulizar.
   Sem modelo confiável (aquecimento das EMAs), usa só a T_in medida. */
static uint8_t bcml_decide(bcml_state_t *s, uint32_t ts, float hour_local, float t_in, float t_pred, int model_ok) {
  float drive = (model_ok && t_pred > t_in) ? t_pred : t_in;
  float held = s->pump_since ? (float)(ts - s->pump_since) / 60.0f : 1e9f;
  if (hour_local < BCML_DAY_H0 || hour_local >= BCML_DAY_H1) {
    if (s->pump_on) { s->pump_on = 0; s->pump_since = ts; }
  } else if (!s->pump_on && drive >= BCML_T_ON && held >= BCML_MIN_OFF_MIN) {
    s->pump_on = 1; s->pump_since = ts;
  } else if (s->pump_on && drive <= BCML_T_OFF && held >= BCML_MIN_ON_MIN) {
    s->pump_on = 0; s->pump_since = ts;
  }
  return s->pump_on;
}

#endif /* BEECOOLER_TINYML_H */
"""

PARITY_C = r"""/* Teste de paridade C x Python:
   gcc -std=c99 -O2 -I../../../../lib/BeeCoolerML/src parity_test.c -lm -o parity && ./parity test_vectors.csv */
#include <stdio.h>
#include <stdlib.h>
#include "beecooler_tinyml.h"

int main(int argc, char **argv) {
  FILE *fp = fopen(argc > 1 ? argv[1] : "test_vectors.csv", "r");
  char line[512];
  bcml_state_t st; bcml_feat_t f;
  long seg_prev = -999, n = 0, n_bad = 0, n_pump_bad = 0;
  double max_fc = 0, max_vs = 0, max_fc_q = 0;
  if (!fp) { printf("sem arquivo\n"); return 2; }
  fgets(line, sizeof line, fp); /* cabeçalho */
  bcml_reset(&st);
  while (fgets(line, sizeof line, fp)) {
    long seg; unsigned long ts; float tin, rhin, tout, rhout, hour, e_fc, e_vs, e_fc_q; int e_pump;
    float pfc, pvs, pq; uint8_t pump; double d;
    if (sscanf(line, "%ld,%lu,%f,%f,%f,%f,%f,%f,%f,%f,%d", &seg, &ts, &tin, &rhin, &tout, &rhout, &hour,
               &e_fc, &e_vs, &e_fc_q, &e_pump) != 11) continue;
    if (seg != seg_prev) { bcml_reset(&st); seg_prev = seg; }
    bcml_update(&st, (uint32_t)ts, tin, rhin, tout, rhout, hour, &f);
    pfc = bcml_forecast(&f, 0); pvs = bcml_virtual_tin(&f, 0); pq = bcml_forecast(&f, 1);
    pump = bcml_decide(&st, (uint32_t)ts, hour, tin, pfc, f.age_min >= BCML_WARMUP_MIN);
    d = fabs(pfc - e_fc); if (d > max_fc) max_fc = d;
    d = fabs(pvs - e_vs); if (d > max_vs) max_vs = d;
    d = fabs(pq - e_fc_q); if (d > max_fc_q) max_fc_q = d;
    if (fabs(pfc - e_fc) > 1e-3 || fabs(pvs - e_vs) > 1e-3) n_bad++;
    if (pump != e_pump) n_pump_bad++;
    n++;
  }
  printf("amostras=%ld  max|dif| previsao=%.2e  sensor_virtual=%.2e  previsao_int8=%.2e  divergencias=%ld  bomba_divergente=%ld\n",
         n, max_fc, max_vs, max_fc_q, n_bad, n_pump_bad);
  return (n_bad == 0 && n_pump_bad == 0) ? 0 : 1;
}
"""


def _write_vectors_h(tv: pd.DataFrame, path: Path):
    """Vetores de teste como arrays C (o teste nativo não depende de arquivo externo)."""
    path.parent.mkdir(parents=True, exist_ok=True)
    nl = "\n"
    rows = ("," + nl).join(
        f"  {{{int(r.ts)}u, {r.t_in:.2f}f, {r.rh_in:.2f}f, {r.t_out:.2f}f, {r.rh_out:.2f}f, "
        f"{r.exp_fc:.6f}f, {r.exp_vs:.6f}f, {int(r.exp_pump)}}}"
        for r in tv.itertuples())
    lines = [
        "/* Gerado por analysis/ml/run_all.py — NÃO EDITAR À MÃO.",
        " * Registros utilizáveis da coleta de campo N01 (tiúba, 22–27/11/2026) e as",
        " * saídas esperadas calculadas em Python (previsão +30 min, sensor virtual, bomba). */",
        "#ifndef BEECOOLER_TINYML_VECTORS_H",
        "#define BEECOOLER_TINYML_VECTORS_H",
        "#include <stdint.h>",
        "",
        "typedef struct { uint32_t ts; float t_in, rh_in, t_out, rh_out, exp_fc, exp_vs; int exp_pump; } bcml_vec_t;",
        f"static const int BCML_N_VEC = {len(tv)};",
        f"static const bcml_vec_t BCML_VEC[{len(tv)}] = {{",
        rows,
        "};",
        "",
        "#endif",
        "",
    ]
    path.write_text(nl.join(lines), encoding="utf-8")


def _choose(task, cfg, res):
    pick = cfg.MODEL_CHOICE.get(task, "auto")
    if pick != "auto":
        return pick
    lin, net = res[task]["linear"]["geral"]["mae"], res[task]["mlp"]["geral"]["mae"]
    return "mlp" if net <= lin * (1 - cfg.MLP_MIN_GAIN) else "linear"


def export_all(X: pd.DataFrame, cfg, out: Path, res: dict) -> dict:
    cdir = out / "c"
    cdir.mkdir(parents=True, exist_ok=True)
    taus = list(cfg.EMA_TAUS_MIN)
    D = X[X.age_min >= cfg.WARMUP_MIN]
    Dfc = D[D.dt_in_fut.notna()]
    kinds = {t: _choose(t, cfg, res) for t in ("FC", "VS")}
    make = lambda k: mlp(cfg.RANDOM_STATE, cfg.MLP_HIDDEN, cfg.MLP_ALPHA) if k == "mlp" else linear()
    fc = make(kinds["FC"]).fit(Dfc[FC_FEATURES], Dfc.dt_in_fut)
    vs = make(kinds["VS"]).fit(D[VS_FEATURES], D.t_in)
    nf, nv = _extract(fc), _extract(vs)
    # conferência: rede extraída == pipeline sklearn
    assert np.allclose(_forward(nf, Dfc[FC_FEATURES].to_numpy()), fc.predict(Dfc[FC_FEATURES]), atol=1e-6)
    assert np.allclose(_forward(nv, D[VS_FEATURES].to_numpy()), vs.predict(D[VS_FEATURES]), atol=1e-6)

    lab = cfg.LABEL
    hdr = ("/* Gerado por BeeCooler/analysis/ml/run_all.py — NÃO EDITAR À MÃO.\n"
           " * Treino: dados de campo N01 (tiúba, quintal), registros utilizáveis após QC.\n"
           " * Limiares 🟡 provisórios (config.LABEL). */\n"
           "#ifndef BEECOOLER_TINYML_H\n#define BEECOOLER_TINYML_H\n#include <stdint.h>\n#include <string.h>\n#include <math.h>\n\n")
    hdr += f"#define BCML_N_TAUS {len(taus)}\nstatic const float BCML_TAUS_MIN[BCML_N_TAUS] = {{{', '.join(f'{t}.0f' for t in taus)}}};\n"
    hdr += f"#define BCML_IDX_EMA60 {taus.index(60)}\n#define BCML_IDX_EMA240 {taus.index(240)}\n"
    hdr += f"#define BCML_HORIZON_MIN {cfg.HORIZON_MIN}\n#define BCML_WARMUP_MIN {cfg.WARMUP_MIN}.0f\n"
    hdr += (f"#define BCML_T_ON {lab['t_on']}f\n#define BCML_T_OFF {lab['t_off']}f\n#define BCML_MIN_ON_MIN {lab['min_on_min']}.0f\n"
            f"#define BCML_MIN_OFF_MIN {lab['min_off_min']}.0f\n#define BCML_DAY_H0 {lab['day_window_h'][0]}.0f\n#define BCML_DAY_H1 {lab['day_window_h'][1]}.0f\n\n")
    s_fc, qf, sf = _emit_net("FC", nf, FC_FEATURES, kinds["FC"])
    s_vs, qv, sv = _emit_net("VS", nv, VS_FEATURES, kinds["VS"])
    hdr += s_fc + s_vs + HEADER_TAIL
    cfg.LIB_ML_DIR.mkdir(parents=True, exist_ok=True)
    (cfg.LIB_ML_DIR / "beecooler_tinyml.h").write_text(hdr, encoding="utf-8")
    (cdir / "parity_test.c").write_text(PARITY_C, encoding="utf-8")

    # vetores de teste: o mesmo caminho de streaming do nó, com decisão
    rows, st, last = [], FeatState(taus=tuple(taus)), None
    Wfq = [q.astype(np.float64) * s for q, s in zip(qf, sf)]
    pump_on, since = False, None
    for r in X.itertuples():
        if r.seg_id != last:
            st.reset(); last = r.seg_id; pump_on, since = False, None
        f = st.update(r.ts, r.t_in, r.rh_in, r.t_out, r.rh_out, r.hour)
        xf = np.array([f[k] for k in FC_FEATURES]); xv = np.array([f[k] for k in VS_FEATURES])
        pfc = r.t_in + _forward(nf, xf); pvs = _forward(nv, xv); pq = r.t_in + _forward(nf, xf, Wfq)
        ok = f["age_min"] >= cfg.WARMUP_MIN
        drive = pfc if ok and pfc > r.t_in else r.t_in
        held = (r.ts - since) / 60 if since else 1e9
        if not (lab["day_window_h"][0] <= r.hour < lab["day_window_h"][1]):
            if pump_on: pump_on, since = False, r.ts
        elif not pump_on and drive >= lab["t_on"] and held >= lab["min_off_min"]:
            pump_on, since = True, r.ts
        elif pump_on and drive <= lab["t_off"] and held >= lab["min_on_min"]:
            pump_on, since = False, r.ts
        rows.append(dict(seg_id=r.seg_id, ts=r.ts, t_in=r.t_in, rh_in=r.rh_in, t_out=r.t_out, rh_out=r.rh_out,
                         hour=round(r.hour, 6), exp_fc=pfc, exp_vs=pvs, exp_fc_q=pq, exp_pump=int(pump_on)))
    tv = pd.DataFrame(rows)
    tv.to_csv(cdir / "test_vectors.csv", index=False, float_format="%.6f")
    _write_vectors_h(tv, cfg.TEST_ML_DIR / "vectors.h")

    # erro da quantização INT8 (no próprio treino) e tamanho
    n_par = lambda n: int(sum(w.size for w in n[2]) + sum(x.size for x in n[3]))
    n_w = lambda n: int(sum(w.size for w in n[2]))
    n_f32 = lambda n: int(sum(x.size for x in n[3]) + 2 * n[0].size + len(n[2]))  # vieses, μ, σ e escalas ficam em float
    out_res = dict(
        modelo_escolhido=kinds,
        fc_parametros=n_par(nf), vs_parametros=n_par(nv),
        bytes_pesos_float32=4 * (n_par(nf) + n_par(nv) + 2 * nf[0].size + 2 * nv[0].size),
        bytes_pesos_int8=n_w(nf) + n_w(nv) + 4 * (n_f32(nf) + n_f32(nv)),
        int8_vs_float_max_dif_C=float(np.max(np.abs(tv.exp_fc_q - tv.exp_fc))),
        treino_amostras_fc=int(len(Dfc)), treino_amostras_vs=int(len(D)),
    )
    # paridade C (se houver gcc)
    res = out_res
    try:
        exe = cdir / "parity.exe"
        subprocess.run(["gcc", "-std=c99", "-O2", f"-I{cfg.LIB_ML_DIR}", "parity_test.c", "-lm", "-o", exe.name], cwd=cdir, check=True,
                       capture_output=True, text=True)
        p = subprocess.run([str(exe), "test_vectors.csv"], cwd=cdir, capture_output=True, text=True)
        res["paridade_c"] = p.stdout.strip() + (" OK" if p.returncode == 0 else " FALHOU")
    except (OSError, subprocess.CalledProcessError) as e:
        res["paridade_c"] = f"não executado: {getattr(e, 'stderr', e)}"
    return res
