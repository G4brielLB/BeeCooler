"""Controle de qualidade: marca (não apaga) cada registro com o motivo de exclusão.

Regras (parâmetros em config.QC):
  - CRC16 inválido;
  - modo TEST_FAST (período <= 60 s): registros de bancada antes da instalação;
  - SHT30 interno/externo: flag de falha, T fora da faixa física, RH_ext ~ 0
    (falha observada em 24/11 09:00 e em 27/11 a partir de 06:50, sem flag do firmware);
  - salto de T > max_step_c em 10 min dentro do mesmo segmento;
  - vibração: estouro de FIFO ou pico saturado.
Segmentos: trechos contínuos de dados válidos; quebram em lacuna > segment_gap_s
ou cold boot (o nó perde a RTC RAM, então as EMAs embarcadas também reiniciam).
"""
from __future__ import annotations

import numpy as np
import pandas as pd


def apply_qc(df: pd.DataFrame, qc: dict) -> pd.DataFrame:
    df = df.copy()
    g = df.groupby(["campaign", "node"], sort=False)
    dt_prev = g.ts.diff()
    dt_next = -g.ts.diff(-1)
    df["dt_prev_s"] = dt_prev
    df["is_test_fast"] = (dt_prev.fillna(1e9) <= qc["test_fast_period_s"]) | (dt_next.fillna(1e9) <= qc["test_fast_period_s"])

    lo, hi = qc["t_valid"]
    df["valid_in"] = (~df.f_sht_in_fail & df.t_in.between(lo, hi) & df.rh_in.between(0.5, 100.0))
    df["valid_out"] = (~df.f_sht_out_fail & df.t_out.between(lo, hi) & (df.rh_out > qc["rh_out_min"]) & (df.rh_out <= 100.0))
    df["valid_vib"] = (~df.f_fifo_overrun & ~df.f_adxl_fail & ~df.f_vib_skipped & df.vib_rms_mg.notna()
                       & (df.vib_peak_mg < qc["vib_peak_sat"]))

    base = df.crc_ok & ~df.is_test_fast
    usable = base & df.valid_in & df.valid_out
    # segmentos sobre os registros utilizáveis
    seg = np.zeros(len(df), dtype=int)
    sid, last_ts, last_key = 0, None, None
    for i, (ok, ts, cold, key) in enumerate(zip(usable, df.ts, df.f_cold_boot, zip(df.campaign, df.node))):
        if not ok:
            seg[i] = -1
            continue
        if last_ts is None or key != last_key or ts - last_ts > qc["segment_gap_s"] or cold:
            sid += 1
        seg[i] = sid
        last_ts, last_key = ts, key
    df["seg_id"] = seg

    # saltos dentro do segmento
    step_in = df.where(usable).groupby("seg_id").t_in.diff().abs()
    step_out = df.where(usable).groupby("seg_id").t_out.diff().abs()
    df["step_suspect"] = (step_in > qc["max_step_c"]) | (step_out > qc["max_step_c"])

    reason = np.full(len(df), "", dtype=object)
    for mask, why in [(~df.crc_ok, "crc"), (df.is_test_fast, "test_fast"), (~df.valid_in, "sht_in"),
                      (~df.valid_out, "sht_out"), (df.step_suspect, "salto")]:
        reason = np.where(mask & (reason == ""), why, reason)
    df["qc_reason"] = reason
    df["usable"] = usable & ~df.step_suspect
    df.loc[~df.usable, "seg_id"] = -1
    return df


def qc_summary(df: pd.DataFrame) -> dict:
    # leituras 2 e 3 do mesmo ciclo (a 1ª tende a repetir o ciclo anterior; ver 'leitura1_repete_anterior')
    noise_in = (df.t_in_r1 - df.t_in_r2).abs()
    noise_out = (df.t_out_r1 - df.t_out_r2).abs()
    prev = df.groupby(["campaign", "node"]).t_in_r2.shift()
    rep = (df.t_in_r0 - prev).abs() < 1e-9
    u = df[df.usable]
    return {
        "registros_total": int(len(df)),
        "registros_utilizaveis": int(df.usable.sum()),
        "motivos_exclusao": df.loc[df.qc_reason != "", "qc_reason"].value_counts().to_dict(),
        "vibracao_invalida_entre_utilizaveis": int((~u.valid_vib).sum()),
        "segmentos": int(u.seg_id.nunique()),
        "periodo_local": [str(u.t_local.min()), str(u.t_local.max())],
        "por_dia_utilizaveis": u.groupby(u.t_local.dt.strftime("%Y-%m-%d")).size().to_dict(),
        # As 3 leituras não são independentes: a 1ª repete o ciclo anterior e a 2ª = 3ª.
        # Logo o ruído do SHT30 NÃO é estimável por estes dados (🔴, achado de firmware).
        "sht_leituras_2e3_identicas_frac": round(float(((noise_in < 1e-9) & (noise_out < 1e-9))[df.usable].mean()), 3),
        "sht_leitura1_repete_ciclo_anterior_frac": round(float(rep[df.usable].mean()), 3),
        "vbat_validos": int(df.vbat_mv.notna().sum()),
        "falha_ext_sem_flag_firmware": int(((df.rh_out <= 0.5) & ~df.f_sht_out_fail).sum()),
    }
