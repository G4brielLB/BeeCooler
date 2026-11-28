"""Treino e validação deixando um dia de fora (leave-one-day-out, LODO).

Duas tarefas:
  VS (sensor virtual): T_in(t) a partir só do sensor externo + hora.
  FC (previsão):       T_in(t + h) a partir de T_in, T_ext e tendências.
Baselines obrigatórios (persistência, média horária, linear) ficam ao lado dos
modelos não lineares para que o ganho da IA seja mensurável.
"""
from __future__ import annotations

import numpy as np
import pandas as pd
from sklearn.base import clone
from sklearn.linear_model import Ridge
from sklearn.neural_network import MLPRegressor
from sklearn.pipeline import make_pipeline
from sklearn.preprocessing import StandardScaler
from sklearn.tree import DecisionTreeRegressor

from .features import FC_FEATURES, VS_FEATURES
from .labels import hysteresis


class HourlyClimatology:
    """Baseline: média de T_in por hora do dia no treino."""

    def fit(self, X, y):
        self.m_ = pd.Series(np.asarray(y)).groupby(np.floor(X["hour"].to_numpy())).mean()
        self.g_ = float(np.mean(y))
        return self

    def predict(self, X):
        return pd.Series(np.floor(X["hour"].to_numpy())).map(self.m_).fillna(self.g_).to_numpy()


class Persistence:
    """Baseline: amanhã = hoje (Δ = 0)."""

    def fit(self, X, y):
        return self

    def predict(self, X):
        return np.zeros(len(X))


class TrendExtrap:
    """Baseline: extrapola a tendência dos últimos 30 min (Δ = 3·d_tin_10 médio)."""

    def fit(self, X, y):
        return self

    def predict(self, X):
        return X["d_tin_30"].to_numpy() * 3


def mlp(seed, hidden, alpha):
    return make_pipeline(StandardScaler(), MLPRegressor(hidden_layer_sizes=hidden, alpha=alpha, max_iter=5000,
                                                        learning_rate_init=3e-3, random_state=seed))


def linear():
    return make_pipeline(StandardScaler(), Ridge(alpha=1.0))


def model_zoo(task, seed, hidden, alpha):
    lin = linear()
    tree = DecisionTreeRegressor(max_depth=5, min_samples_leaf=10, random_state=seed)
    zoo = {"linear": lin, "arvore_d5": tree, "mlp": mlp(seed, hidden, alpha)}
    if task == "VS":
        return {"media_horaria": HourlyClimatology(), **zoo}
    return {"persistencia": Persistence(), "tendencia_30min": TrendExtrap(), **zoo}


def _metrics(err, mask=None):
    e = err if mask is None else err[mask]
    e = e[~np.isnan(e)]
    if len(e) == 0:
        return dict(n=0)
    return dict(n=int(len(e)), mae=float(np.mean(np.abs(e))), rmse=float(np.sqrt(np.mean(e ** 2))),
                p95=float(np.percentile(np.abs(e), 95)), max=float(np.max(np.abs(e))), bias=float(np.mean(e)))


def lodo(X: pd.DataFrame, task: str, cfg) -> tuple[pd.DataFrame, dict]:
    feats = VS_FEATURES if task == "VS" else FC_FEATURES
    target = "t_in" if task == "VS" else "dt_in_fut"
    D = X[(X.age_min >= cfg.WARMUP_MIN) & X[target].notna()].copy()
    days = sorted(D.day.unique())
    preds = {}
    for name, proto in model_zoo(task, cfg.RANDOM_STATE, cfg.MLP_HIDDEN, cfg.MLP_ALPHA).items():
        p = pd.Series(np.nan, index=D.index)
        for d in days:
            tr, te = D[D.day != d], D[D.day == d]
            m = clone(proto) if hasattr(proto, "get_params") else proto
            Xtr = tr[feats + ["hour"]] if name == "media_horaria" else tr[feats]
            Xte = te[feats + ["hour"]] if name == "media_horaria" else te[feats]
            m.fit(Xtr, tr[target].to_numpy())
            p.loc[te.index] = m.predict(Xte)
        preds[name] = p
    truth = D["t_in"] if task == "VS" else D["t_in_fut"]
    offset = 0 if task == "VS" else D["t_in"]
    res = {}
    day_mask = D.hour.between(cfg.LABEL["day_window_h"][0], cfg.LABEL["day_window_h"][1], inclusive="left").to_numpy()
    hot_mask = (truth >= cfg.LABEL["t_on"] - 1).to_numpy()
    for name, p in preds.items():
        pt = p + offset
        D[f"pred_{name}"] = pt
        err = (pt - truth).to_numpy()
        res[name] = dict(geral=_metrics(err), diurno=_metrics(err, day_mask), quente=_metrics(err, hot_mask),
                         por_dia={d: _metrics(err, (D.day == d).to_numpy()) for d in days})
    return D, res


def decision_eval(D: pd.DataFrame, pred_col: str, lab: dict, mode: str) -> dict:
    """Compara a bomba comandada pelo modelo com a regra sobre a T_in medida.

    mode="FC": entrada = max(T_in, T_in previsto em +h)  (antecipação)
    mode="VS": entrada = T_in estimada pelo sensor virtual (sem sensor interno)
    Referência: regra sobre T_in medida (reativa) e regra 'ideal antecipada' sobre max(T_in, T_in real em +h).
    """
    ts, hour, seg = D.ts.to_numpy(), D.hour.to_numpy(), D.seg_id.to_numpy()
    ref = hysteresis(ts, D.t_in.to_numpy(), hour, seg, lab)
    if mode == "FC":
        drive = np.maximum(D.t_in.to_numpy(), D[pred_col].to_numpy())
        ideal = hysteresis(ts, np.maximum(D.t_in.to_numpy(), D.t_in_fut.to_numpy()), hour, seg, lab)
    else:
        drive = D[pred_col].to_numpy()
        ideal = ref
    mdl = hysteresis(ts, drive, hour, seg, lab)

    def starts(on):
        s = []
        for g in np.unique(seg):
            m = seg == g
            o = on[m].astype(int)
            idx = np.flatnonzero(np.diff(np.r_[0, o]) == 1)
            s += list(ts[m][idx])
        return np.array(s)

    leads = []
    for s_ref in starts(ref):
        cand = [s for s in starts(mdl) if abs(s - s_ref) <= 3 * 3600]
        if cand:
            leads.append((s_ref - min(cand, key=lambda s: abs(s - s_ref))) / 60)
    tp = int((mdl & ideal).sum()); fp = int((mdl & ~ideal).sum()); fn = int((~mdl & ideal).sum())
    return dict(min_ligada_regra_reativa=int(ref.sum() * 10), min_ligada_ideal=int(ideal.sum() * 10),
                min_ligada_modelo=int(mdl.sum() * 10),
                precisao_vs_ideal=tp / (tp + fp) if tp + fp else None, recall_vs_ideal=tp / (tp + fn) if tp + fn else None,
                acionamentos_regra=int(len(starts(ref))), acionamentos_modelo=int(len(starts(mdl))),
                antecipacao_media_min=float(np.mean(leads)) if leads else None,
                antecipacao_por_episodio_min=[float(x) for x in leads])
