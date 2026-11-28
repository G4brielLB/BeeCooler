"""Roda o pipeline inteiro: ingestão → QC → features/rótulos → figuras → LODO → exportação.

Uso (de BeeCooler/analysis/ml):
    uv venv .venv && uv pip install -r requirements.txt
    .venv/Scripts/python run_all.py          # Windows
Re-treino com mais dados: adicione dias em coleta_dados/ e rode de novo.
"""
from __future__ import annotations

import json

import numpy as np
import pandas as pd

import config as cfg
from beeml import eda, export, features, ingest, labels, qc, train


def jdump(obj, path):
    def conv(o):
        if isinstance(o, (np.floating, np.integer)):
            return o.item()
        raise TypeError(type(o))
    path.write_text(json.dumps(obj, indent=2, ensure_ascii=False, default=conv), encoding="utf-8")


def main():
    out = cfg.OUT
    (out / "figs").mkdir(parents=True, exist_ok=True)

    # 1. ingestão + QC
    raw = ingest.load_all(cfg.DATA_ROOT, cfg.UTC_OFFSET_H)
    df = qc.apply_qc(raw, cfg.QC)
    df.to_csv(out / "registros_qc.csv", index=False)
    summ = qc.qc_summary(df)
    print("[qc]", json.dumps(summ, ensure_ascii=False, default=str))

    # 2. features + rótulos
    X = features.build(df, cfg.EMA_TAUS_MIN, cfg.HORIZON_MIN)
    X = labels.add_labels(X, cfg.LABEL, cfg.HORIZON_MIN)
    X.to_csv(out / "features_rotulos.csv", index=False)
    ep = labels.episodes(X, "pump_rule")
    ep.to_csv(out / "episodios_regra.csv", index=False)

    sens = {}
    for t_on in cfg.SENSITIVITY_T_ON:
        lab = dict(cfg.LABEL, t_on=t_on, t_off=t_on - 1)
        Xs = X.assign(pump_rule=labels.hysteresis(X.ts.to_numpy(), X.t_in.to_numpy(), X.hour.to_numpy(), X.seg_id.to_numpy(), lab))
        e = labels.episodes(Xs, "pump_rule")
        full_days = X.groupby("day").size()
        full_days = full_days[full_days >= 130].index
        e_full = e[e.day.isin(full_days)]
        sens[str(t_on)] = dict(episodios=int(len(e)),
                               min_ligada_por_dia_completo=float(e_full.groupby("day").duracao_min.sum().reindex(full_days, fill_value=0).mean()) if len(full_days) else None,
                               inicio_mediano=sorted(e.inicio)[len(e) // 2] if len(e) else None)

    # 3. figuras exploratórias
    eda.fig_series(df, out / "figs/01_serie_qc.png")
    eda.fig_hourly(X, out / "figs/02_perfil_horario.png")
    eda.fig_inertia(X[X.age_min >= cfg.WARMUP_MIN], out / "figs/03_inercia.png")
    eda.fig_vibration(X, out / "figs/04_vibracao.png")
    eda.fig_labels(X, cfg.LABEL, out / "figs/05_rotulos.png")

    # 4. modelos (LODO)
    res = {}
    D_vs, res["VS"] = train.lodo(X, "VS", cfg)
    D_fc, res["FC"] = train.lodo(X, "FC", cfg)
    res["decisao"] = {
        "FC_mlp": train.decision_eval(D_fc, "pred_mlp", cfg.LABEL, "FC"),
        "FC_linear": train.decision_eval(D_fc, "pred_linear", cfg.LABEL, "FC"),
        "FC_persistencia": train.decision_eval(D_fc, "pred_persistencia", cfg.LABEL, "FC"),
        "VS_mlp": train.decision_eval(D_vs, "pred_mlp", cfg.LABEL, "VS"),
        "VS_linear": train.decision_eval(D_vs, "pred_linear", cfg.LABEL, "VS"),
    }
    D_vs.to_csv(out / "pred_lodo_sensor_virtual.csv", index=False)
    D_fc.to_csv(out / "pred_lodo_previsao.csv", index=False)
    eda.fig_pred(D_vs, [("pred_mlp", "MLP"), ("pred_linear", "linear"), ("pred_media_horaria", "média horária")],
                 "Sensor virtual: T_in estimada só com o sensor externo (validação deixando o dia de fora)",
                 out / "figs/06_sensor_virtual.png")
    eda.fig_pred(D_fc, [("pred_mlp", "MLP"), ("pred_persistencia", "persistência")],
                 f"Previsão de T_in em +{cfg.HORIZON_MIN} min (validação deixando o dia de fora)",
                 out / "figs/07_previsao.png", truth="t_in_fut")

    # 5. modelo final (todos os dados) + exportação para C
    exp = export.export_all(X, cfg, out, res)

    jdump(dict(qc=summ, sensibilidade_t_on=sens, episodios_regra=ep.to_dict("records"), modelos=res, exportacao=exp),
          out / "metricas.json")
    for task in ("VS", "FC"):
        print(f"\n[{task}] LODO")
        for name, r in res[task].items():
            g, d = r["geral"], r["diurno"]
            print(f"  {name:16s} MAE={g['mae']:.3f}  P95={g['p95']:.2f}  max={g['max']:.2f}  | diurno MAE={d['mae']:.3f}")
    print("\n[decisão]")
    for k, v in res["decisao"].items():
        print(f"  {k:16s}", {kk: (round(vv, 3) if isinstance(vv, float) else vv) for kk, vv in v.items() if kk != 'antecipacao_por_episodio_min'})
    print("\n[sensibilidade t_on]", sens)
    print("\n[exportação]", exp)


if __name__ == "__main__":
    main()
