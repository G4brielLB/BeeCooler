"""Features calculáveis no nó, uma amostra por vez (espelho do que roda no ESP32-S3).

O estado (`FeatState`) é pequeno e cabe na RTC RAM (D32): 4 EMAs de T_ext,
1 EMA de RH_ext, as 3 últimas T_in/T_ext e o timestamp anterior. Ele é
reiniciado no início de cada segmento (lacuna longa ou cold boot), exatamente
como acontece no nó quando a RTC RAM se perde.

EMA com passo irregular: alpha = 1 - exp(-dt/tau), então os ciclos noturnos de
30 min (D35) e eventuais ciclos perdidos são tratados corretamente.
"""
from __future__ import annotations

import math
from dataclasses import dataclass, field

import numpy as np
import pandas as pd

# Conjuntos de entrada dos modelos
VS_FEATURES = ["t_out", "rh_out", "ema_out_30", "ema_out_60", "ema_out_120", "ema_out_240", "ema_rh_out_60",
               "h_sin", "h_cos"]                                  # sensor virtual: SEM T_in
FC_FEATURES = ["t_in", "d_tin_10", "d_tin_30", "rh_in", "t_out", "d_tout_30", "ema_out_60", "ema_out_240",
               "h_sin", "h_cos"]                                  # previsão de T_in(+h)


@dataclass
class FeatState:
    taus: tuple
    ts: float | None = None
    ema_out: list = field(default_factory=list)
    ema_rh: float = 0.0
    tin_hist: list = field(default_factory=list)   # mais recente por último, até 4
    tout_hist: list = field(default_factory=list)
    ts_hist: list = field(default_factory=list)
    age_s: float = 0.0                              # tempo desde o reset (aquecimento das EMAs)

    def reset(self):
        self.ts = None
        self.ema_out, self.ema_rh = [], 0.0
        self.tin_hist, self.tout_hist, self.ts_hist = [], [], []
        self.age_s = 0.0

    def update(self, ts, t_in, rh_in, t_out, rh_out, hour_local):
        if self.ts is None:
            self.ema_out = [t_out] * len(self.taus)
            self.ema_rh = rh_out
        else:
            dt = ts - self.ts
            self.age_s += dt
            self.ema_out = [e + (1 - math.exp(-dt / (tau * 60))) * (t_out - e) for e, tau in zip(self.ema_out, self.taus)]
            self.ema_rh += (1 - math.exp(-dt / 3600)) * (rh_out - self.ema_rh)
        self.ts = ts
        for hist, v in ((self.tin_hist, t_in), (self.tout_hist, t_out), (self.ts_hist, ts)):
            hist.append(v)
            del hist[:-4]
        f = {"t_in": t_in, "rh_in": rh_in, "t_out": t_out, "rh_out": rh_out, "ema_rh_out_60": self.ema_rh}
        for tau, e in zip(self.taus, self.ema_out):
            f[f"ema_out_{tau}"] = e
        f["d_tin_10"] = self._slope(self.tin_hist, 1)
        f["d_tin_30"] = self._slope(self.tin_hist, 3)
        f["d_tout_30"] = self._slope(self.tout_hist, 3)
        f["h_sin"] = math.sin(2 * math.pi * hour_local / 24)
        f["h_cos"] = math.cos(2 * math.pi * hour_local / 24)
        f["age_min"] = self.age_s / 60
        return f

    def _slope(self, hist, k):
        """Variação em °C por 10 min, usando a amostra k passos atrás (ou a mais antiga disponível)."""
        if len(hist) < 2:
            return 0.0
        j = max(0, len(hist) - 1 - k)
        dt = self.ts_hist[-1] - self.ts_hist[j]
        return (hist[-1] - hist[j]) / (dt / 600) if dt > 0 else 0.0


def build(df: pd.DataFrame, taus, horizon_min: int) -> pd.DataFrame:
    """Tabela de features + alvos, só com registros utilizáveis."""
    u = df[df.usable].copy()
    st = FeatState(taus=tuple(taus))
    rows = []
    last_seg = None
    for r in u.itertuples():
        if r.seg_id != last_seg:
            st.reset()
            last_seg = r.seg_id
        hour = r.t_local.hour + r.t_local.minute / 60
        f = st.update(r.ts, r.t_in, r.rh_in, r.t_out, r.rh_out, hour)
        f.update(ts=r.ts, t_local=r.t_local, seg_id=r.seg_id, node=r.node, day=r.t_local.strftime("%Y-%m-%d"),
                 hour=hour, vib_rms_mg=r.vib_rms_mg if r.valid_vib else np.nan,
                 **{f"band{k}_db": getattr(r, f"band{k}_db") if r.valid_vib else np.nan for k in range(4)})
        rows.append(f)
    X = pd.DataFrame(rows)
    # alvo: T_in exatamente em ts + horizonte, no mesmo segmento
    key = X.set_index(["seg_id", "ts"]).t_in
    tgt = [key.get((s, t + horizon_min * 60), np.nan) for s, t in zip(X.seg_id, X.ts)]
    X[f"t_in_fut"] = tgt
    X["dt_in_fut"] = X.t_in_fut - X.t_in
    return X
