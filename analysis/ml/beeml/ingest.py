"""Ingestão: decodifica os `rec.bin` (ExtendedRecord de 64 B) da microSD.

Layout em lib/BeeCoolerRecord/src/BeeCoolerRecord.h (little-endian):
  seq u32 | RecordV1 (24 B) | sht_raw[12] u16 | wake_ms u16 | sd_write_ms u16 |
  processing_ms u16 | fifo_overruns u8 | vbat_tx_mv u16 | reserved u8 | crc u16
CRC16 CCITT-FALSE sobre os 62 primeiros bytes.

Os arquivos originais não são alterados; a saída é uma tabela nova.
"""
from __future__ import annotations

import struct
from pathlib import Path

import numpy as np
import pandas as pd

REC_SIZE = 64
FMT = "<I" + "IhHhHHHH4BH" + "12H" + "HHHBHBH"
assert struct.calcsize(FMT) == REC_SIZE

I16_INV, U16_INV = -32768, 0xFFFF


def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def _sht_t(raw):  # sht_raw guarda T já convertida: int16 em 0,01 °C (Sht30Pair.cpp: raw = tc)
    t = raw.astype(np.uint16).astype(np.int16) / 100.0
    return np.where((raw == U16_INV) | (t < -40) | (t > 125), np.nan, t)


def _sht_rh(raw):  # RH em 0,01 %
    rh = raw / 100.0
    return np.where((raw == U16_INV) | (rh > 100), np.nan, rh)


def find_days(data_root: Path) -> list[Path]:
    """Toda pasta <...>/BEE/<nó>/<AAAAMMDD>/ que tenha rec.bin."""
    return sorted(p.parent for p in data_root.rglob("rec.bin") if p.parent.parent.parent.name == "BEE")


def read_day(day_dir: Path) -> pd.DataFrame:
    raw = day_dir.joinpath("rec.bin").read_bytes()
    n = len(raw) // REC_SIZE
    rows = []
    for i in range(n):
        chunk = raw[i * REC_SIZE:(i + 1) * REC_SIZE]
        v = struct.unpack(FMT, chunk)
        rows.append(v + (crc16_ccitt_false(chunk[:62]) == v[-1],))
    cols = (["seq", "ts", "t_in_c", "rh_in_c", "t_out_c", "rh_out_c", "vbat_mv", "vib_rms_c", "vib_peak_c",
             "b0", "b1", "b2", "b3", "flags"] + [f"sht_raw{k}" for k in range(12)] +
            ["wake_ms", "sd_write_ms", "processing_ms", "fifo_overruns", "vbat_tx_mv", "reserved", "crc", "crc_ok"])
    df = pd.DataFrame(rows, columns=cols)
    df["campaign"] = day_dir.parents[2].name
    df["node"] = day_dir.parent.name
    df["day_dir"] = day_dir.name
    df["src_offset"] = np.arange(n) * REC_SIZE
    if len(raw) % REC_SIZE:
        print(f"[ingest] aviso: {day_dir} tem {len(raw) % REC_SIZE} bytes sobrando (registro truncado)")
    return df


def decode(df: pd.DataFrame, utc_offset_h: int) -> pd.DataFrame:
    out = pd.DataFrame({
        "campaign": df.campaign, "node": df.node, "day_dir": df.day_dir, "src_offset": df.src_offset,
        "seq": df.seq, "ts": df.ts, "crc_ok": df.crc_ok, "flags": df["flags"],
    })
    out["t_utc"] = pd.to_datetime(df.ts, unit="s", utc=True)
    out["t_local"] = (out.t_utc + pd.Timedelta(hours=utc_offset_h)).dt.tz_localize(None)
    for name, col, inv, scale in [("t_in", "t_in_c", I16_INV, 0.01), ("rh_in", "rh_in_c", U16_INV, 0.01),
                                  ("t_out", "t_out_c", I16_INV, 0.01), ("rh_out", "rh_out_c", U16_INV, 0.01),
                                  ("vib_rms_mg", "vib_rms_c", U16_INV, 0.1), ("vib_peak_mg", "vib_peak_c", U16_INV, 0.1),
                                  ("vbat_mv", "vbat_mv", U16_INV, 1.0)]:
        out[name] = np.where(df[col] == inv, np.nan, df[col] * scale)
    for k in range(4):  # band_db = round(2·(dB+100)), 0xFF inválido
        out[f"band{k}_db"] = np.where(df[f"b{k}"] == 0xFF, np.nan, df[f"b{k}"] / 2.0 - 100.0)
    # 3 leituras individuais por sensor (s=0 interno, s=1 externo): índice s*6 + k*2 (+1 para RH).
    # O registro guarda a mediana das 3.
    for s, tag in ((0, "in"), (1, "out")):
        for k in range(3):
            out[f"t_{tag}_r{k}"] = _sht_t(df[f"sht_raw{s * 6 + k * 2}"].to_numpy())
            out[f"rh_{tag}_r{k}"] = _sht_rh(df[f"sht_raw{s * 6 + k * 2 + 1}"].to_numpy())
    out["fifo_overruns"] = df.fifo_overruns
    out["wake_ms"] = df.wake_ms
    for bit, name in [(0, "f_sht_in_fail"), (1, "f_sht_out_fail"), (2, "f_adxl_fail"), (3, "f_fifo_overrun"),
                      (4, "f_rtc_invalid"), (5, "f_clock_adjusted"), (6, "f_cold_boot"), (7, "f_sd_fail"),
                      (8, "f_vib_skipped"), (11, "f_sht_in_stuck"), (12, "f_mech_event")]:
        out[name] = ((df["flags"].to_numpy().astype(np.int64) >> bit) & 1) == 1
    return out


def load_all(data_root: Path, utc_offset_h: int) -> pd.DataFrame:
    days = find_days(data_root)
    if not days:
        raise SystemExit(f"nenhum rec.bin encontrado em {data_root}")
    df = decode(pd.concat([read_day(d) for d in days], ignore_index=True), utc_offset_h)
    df = df.sort_values(["campaign", "node", "ts", "seq"]).reset_index(drop=True)
    return df
