# -*- coding: utf-8 -*-
"""
exporta_mensagens.py - do corpo enviado à API ao CSV do conjunto de dados e ao
CSV no formato do gravador da disciplina (para rodar o qualidade.py sem alteração).

    python tools/exporta_mensagens.py nuvem/corpo_boot234.json --saida dados

Gera, em --saida:
  sessao_20261005_n01.csv        uma linha por registro, campos decodificados (Seção 5.8)
  mensagens_sessao_20261005.csv  colunas t_chegada, chegada, topico, retido, payload,
                                 como o gravador.py do Laboratório 03

O registro canônico v1 tem 24 B (little-endian) e é decodificado exatamente como
no record.py da Lambda de escrita. Sentinelas viram campo vazio.

A "chegada" é o instante em que o lote chegou a quem guarda o dado (a API), ou seja,
o gw_time do corpo enviado: nesta sessão o envio foi feito à mão, a partir do
notebook, em 19/10/2026. O log serial da central não registra hora de chegada.
"""
import argparse, base64, csv, json, os, struct
from datetime import datetime, timezone

REC = struct.Struct('<IhHhHHHH4BH')
assert REC.size == 24
I16_NA, U16_NA, U8_NA = -32768, 0xFFFF, 0xFF
TOPICO = "ufpi-iot26/beecooler/n01/telemetria"   # nominal: só para o qualidade.py reconhecer

def v(x, na, k, nd):
    return None if x == na else round(x * k, nd)

def decode(buf):
    ts, ti, ri, to, ro, vb, rms, pk, b0, b1, b2, b3, fl = REC.unpack(buf)
    bands = [None if b == U8_NA else b / 2 - 100 for b in (b0, b1, b2, b3)]
    return {
        "ts": ts, "t_in": v(ti, I16_NA, .01, 2), "rh_in": v(ri, U16_NA, .01, 2),
        "t_out": v(to, I16_NA, .01, 2), "rh_out": v(ro, U16_NA, .01, 2),
        "vbat": v(vb, U16_NA, .001, 3), "vib_rms": v(rms, U16_NA, .1, 1),
        "vib_peak": v(pk, U16_NA, .1, 1),
        "band_50_100": bands[0], "band_100_200": bands[1],
        "band_200_350": bands[2], "band_350_600": bands[3], "flags": fl,
    }

p = argparse.ArgumentParser()
p.add_argument("corpo")
p.add_argument("--saida", default="dados")
a = p.parse_args()
corpo = json.load(open(a.corpo, encoding="utf-8"))
chegada = corpo["gw_time"]
os.makedirs(a.saida, exist_ok=True)

linhas = []
for s in corpo["sessions"]:
    k = 0
    for run in s["runs"]:
        raw = base64.b64decode(run["data"])
        for i in range(len(raw) // 24):
            d = decode(raw[24 * i: 24 * (i + 1)])
            linhas.append({"node_id": s["node_id"], "boot_id": s["boot_id"],
                           "seq": run["first_seq"] + i,
                           "attempt": s.get("attempts", [None] * 999)[k], **d})
            k += 1

campos = ["node_id", "boot_id", "seq", "ts", "ts_utc", "t_in", "rh_in", "t_out", "rh_out",
          "vbat", "vib_rms", "vib_peak", "band_50_100", "band_100_200", "band_200_350",
          "band_350_600", "flags", "attempt", "chegada"]
utc = lambda t: datetime.fromtimestamp(t, timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
with open(os.path.join(a.saida, "sessao_20261005_n01.csv"), "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=campos)
    w.writeheader()
    for l in linhas:
        w.writerow({**l, "ts_utc": utc(l["ts"]), "chegada": chegada})

with open(os.path.join(a.saida, "mensagens_sessao_20261005.csv"), "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow(["t_chegada", "chegada", "topico", "retido", "payload"])
    hora = datetime.fromtimestamp(chegada).strftime("%H:%M:%S")
    for l in linhas:
        pl = {"id": "beecooler-n%02d" % l["node_id"], "boot": l["boot_id"], "seq": l["seq"],
              "ts": l["ts"], **{c: l[c] for c in campos[5:17]}}
        w.writerow([f"{chegada:.3f}", hora, TOPICO, 0, json.dumps(pl, separators=(",", ":"))])

print(f"{len(linhas)} registros; chegada = {utc(chegada)}")
