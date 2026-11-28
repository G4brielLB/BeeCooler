"""Figuras da análise exploratória e dos resultados (PNG estático, tema claro)."""
from __future__ import annotations

import matplotlib

matplotlib.use("Agg")
import matplotlib.dates as mdates
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

# Paleta categórica validada (ordem fixa): interna, externa, modelo, ...
C_IN, C_OUT, C_MDL, C_4 = "#2a78d6", "#eb6834", "#1baf7a", "#eda100"
C_CRIT, C_MUTED, SURF = "#d03b3b", "#52514e", "#fcfcfb"

plt.rcParams.update({
    "figure.facecolor": SURF, "axes.facecolor": SURF, "savefig.facecolor": SURF, "font.size": 9,
    "axes.edgecolor": "#c9c8c3", "axes.labelcolor": "#0b0b0b", "xtick.color": C_MUTED, "ytick.color": C_MUTED,
    "axes.grid": True, "grid.color": "#e6e5e0", "grid.linewidth": 0.6, "axes.spines.top": False,
    "axes.spines.right": False, "lines.linewidth": 1.6, "legend.frameon": False,
})


def _fmt_time(ax):
    ax.xaxis.set_major_formatter(mdates.DateFormatter("%d/%m\n%H:%M"))


def fig_series(df, path):
    """Série completa com os pontos excluídos pelo QC."""
    fig, axs = plt.subplots(2, 1, figsize=(11, 6), sharex=True)
    d = df[~df.is_test_fast]
    for ax, (a, b, unit) in zip(axs, [("t_in", "t_out", "Temperatura (°C)"), ("rh_in", "rh_out", "Umidade relativa (%)")]):
        u = d[d.usable]
        ax.plot(u.t_local, u[a], ".", ms=2.5, color=C_IN, label="interna")
        ax.plot(u.t_local, u[b], ".", ms=2.5, color=C_OUT, label="externa")
        x = d[~d.usable]
        ax.plot(x.t_local, x[b], "x", ms=5, color=C_CRIT, label="excluído no QC (externa)")
        ax.set_ylabel(unit)
        ax.legend(loc="upper left", ncol=3)
    _fmt_time(axs[-1])
    axs[0].set_title("N01 · campo (tiúba, quintal) · registros de 10 min", loc="left")
    fig.tight_layout(); fig.savefig(path, dpi=130); plt.close(fig)


def fig_hourly(X, path):
    """Perfil médio por hora: T_in, T_ext e ΔT (dois painéis, um eixo cada)."""
    h = X.assign(hh=np.floor(X.hour)).groupby("hh")
    fig, axs = plt.subplots(1, 2, figsize=(11, 3.6))
    for col, c, lab in [("t_in", C_IN, "interna"), ("t_out", C_OUT, "externa")]:
        m, s = h[col].mean(), h[col].std()
        axs[0].plot(m.index, m, color=c, label=lab)
        axs[0].fill_between(m.index, m - s, m + s, color=c, alpha=0.15, lw=0)
    axs[0].set_xlabel("hora local"); axs[0].set_ylabel("°C"); axs[0].legend(); axs[0].set_title("Média ± desvio por hora", loc="left")
    dT = h.apply(lambda g: (g.t_in - g.t_out).mean())
    axs[1].bar(dT.index, dT, color=np.where(dT >= 0, C_IN, C_OUT), width=0.8)
    axs[1].axhline(0, color=C_MUTED, lw=0.8)
    axs[1].set_xlabel("hora local"); axs[1].set_ylabel("T_in − T_ext (°C)")
    axs[1].set_title("Interna acima (azul) ou abaixo (laranja) da externa", loc="left")
    fig.tight_layout(); fig.savefig(path, dpi=130); plt.close(fig)


def fig_inertia(X, path):
    fig, axs = plt.subplots(1, 2, figsize=(10, 4), sharey=True)
    for ax, col, title in [(axs[0], "t_out", "T_ext instantânea"), (axs[1], "ema_out_120", "T_ext suavizada (EMA τ = 120 min)")]:
        r = np.corrcoef(X[col], X.t_in)[0, 1]
        ax.scatter(X[col], X.t_in, s=6, color=C_IN, alpha=0.5)
        ax.set_xlabel(f"{col} (°C)"); ax.set_title(f"{title} · r = {r:.2f}", loc="left")
    axs[0].set_ylabel("T_in (°C)")
    fig.tight_layout(); fig.savefig(path, dpi=130); plt.close(fig)


def fig_vibration(X, path):
    v = X[X.vib_rms_mg.notna()]
    h = v.assign(hh=np.floor(v.hour)).groupby("hh")
    fig, axs = plt.subplots(1, 2, figsize=(11, 3.6))
    axs[0].plot(v.t_local, v.vib_rms_mg, ".", ms=2.5, color=C_IN)
    axs[0].set_ylabel("RMS (mg)"); axs[0].set_title("Vibração RMS (registros válidos)", loc="left"); _fmt_time(axs[0])
    for k, c in zip(range(4), [C_IN, C_OUT, C_MDL, C_4]):
        m = h[f"band{k}_db"].mean()
        axs[1].plot(m.index, m, color=c, label=["50–100", "100–200", "200–350", "350–600"][k] + " Hz")
    axs[1].set_xlabel("hora local"); axs[1].set_ylabel("dB"); axs[1].legend(fontsize=8)
    axs[1].set_title("Energia por banda, média por hora", loc="left")
    fig.tight_layout(); fig.savefig(path, dpi=130); plt.close(fig)


def fig_labels(X, lab, path):
    fig, ax = plt.subplots(figsize=(11, 3.6))
    for seg, g in X.groupby("seg_id"):
        ax.plot(g.t_local, g.t_in, color=C_IN, lw=1.4, label="T_in" if seg == X.seg_id.min() else None)
        on = g.pump_rule.to_numpy()
        ax.fill_between(g.t_local, 0, 1, where=on, transform=ax.get_xaxis_transform(), color=C_MDL, alpha=0.18, lw=0,
                        label="bomba ligada (regra)" if seg == X.seg_id.min() else None)
    ax.axhline(lab["t_on"], color=C_CRIT, lw=1, ls="--", label=f"t_on = {lab['t_on']} °C")
    ax.axhline(lab["t_off"], color=C_MUTED, lw=1, ls=":", label=f"t_off = {lab['t_off']} °C")
    ax.set_ylabel("°C"); ax.legend(loc="lower left", ncol=4); _fmt_time(ax)
    ax.set_title("Rótulo por regra de histerese (sem atuação real na coleta)", loc="left")
    fig.tight_layout(); fig.savefig(path, dpi=130); plt.close(fig)


def fig_pred(D, cols, title, path, truth="t_in"):
    fig, ax = plt.subplots(figsize=(11, 3.8))
    for seg, g in D.groupby("seg_id"):
        first = seg == D.seg_id.min()
        ax.plot(g.t_local, g[truth], color=C_IN, lw=1.6, label="medida" if first else None)
        for (col, lab), c in zip(cols, [C_MDL, C_OUT, C_4]):
            ax.plot(g.t_local, g[col], color=c, lw=1.2, label=lab if first else None)
    ax.set_ylabel("T_in (°C)"); ax.legend(loc="lower left", ncol=4); _fmt_time(ax)
    ax.set_title(title, loc="left")
    fig.tight_layout(); fig.savefig(path, dpi=130); plt.close(fig)
