"""Rótulos de atuação (supervisão fraca por regra) e camada de decisão.

Não há rótulo verdadeiro: a bomba nunca ligou na coleta. O rótulo é o que uma
regra de histerese sobre T_in teria feito, com limiares provisórios (config.LABEL).
"Quando ligar" e "por quanto tempo" saem do mesmo autômato:

  DESLIGADA --(T >= t_on e desligada há >= min_off)--> LIGADA
  LIGADA    --(T <= t_off e ligada há >= min_on)----> DESLIGADA
  fora da janela diurna -> DESLIGADA

Com o modelo, a entrada do autômato passa a ser max(T_in, T_in previsto em +h),
o que antecipa o acionamento. A duração da dose real (efeito da nebulização na
T_in) só pode ser calibrada com o ensaio da bomba (🔴, Fase 2).
"""
from __future__ import annotations

import numpy as np
import pandas as pd


def hysteresis(ts, temp, hour, seg, lab: dict) -> np.ndarray:
    on = np.zeros(len(ts), dtype=bool)
    state, since, last_seg = False, None, None
    h0, h1 = lab["day_window_h"]
    for i in range(len(ts)):
        if seg[i] != last_seg:  # início de segmento: sem restrição de tempo mínimo desligada
            state, since, last_seg = False, ts[i] - 1e9, seg[i]
        held = (ts[i] - since) / 60
        if not (h0 <= hour[i] < h1):
            if state:
                state, since = False, ts[i]
        elif not state and temp[i] >= lab["t_on"] and held >= lab["min_off_min"]:
            state, since = True, ts[i]
        elif state and temp[i] <= lab["t_off"] and held >= lab["min_on_min"]:
            state, since = False, ts[i]
        on[i] = state
    return on


def add_labels(X: pd.DataFrame, lab: dict, horizon_min: int) -> pd.DataFrame:
    X = X.copy()
    X["pump_rule"] = hysteresis(X.ts.to_numpy(), X.t_in.to_numpy(), X.hour.to_numpy(), X.seg_id.to_numpy(), lab)
    # rótulo antecipado: T_in vai cruzar t_on dentro do horizonte?
    fut_max = []
    by_seg = X.groupby("seg_id")
    for _, g in by_seg:
        ts, t = g.ts.to_numpy(), g.t_in.to_numpy()
        for i in range(len(g)):
            m = (ts > ts[i]) & (ts <= ts[i] + horizon_min * 60)
            fut_max.append(t[m].max() if m.any() else np.nan)
    X["t_in_max_fut"] = fut_max
    X["y_hot_fut"] = (X.t_in_max_fut >= lab["t_on"]).astype(float).where(X.t_in_max_fut.notna())
    return X


def episodes(X: pd.DataFrame, col: str) -> pd.DataFrame:
    """Um episódio por trecho contínuo de bomba ligada: início, fim, duração, T_in máx."""
    out = []
    for seg, g in X.groupby("seg_id"):
        on = g[col].to_numpy()
        idx = np.flatnonzero(np.diff(np.r_[0, on.astype(int), 0]))
        for a, b in zip(idx[::2], idx[1::2]):
            gg = g.iloc[a:b]
            dur = (gg.ts.iloc[-1] - gg.ts.iloc[0]) / 60 + 10
            out.append(dict(seg_id=seg, day=gg.day.iloc[0], inicio=gg.t_local.iloc[0].strftime("%H:%M"),
                            fim=gg.t_local.iloc[-1].strftime("%H:%M"), duracao_min=dur, t_in_max=gg.t_in.max(),
                            t_out_max=gg.t_out.max()))
    return pd.DataFrame(out)
