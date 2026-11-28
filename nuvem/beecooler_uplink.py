#!/usr/bin/env python3
"""
beecooler_uplink.py — converte logs seriais da central/nó em registros canônicos v1
(24 B) e envia ao POST /v1/telemetry no formato do contrato da Integração v1.1 (§5.2).

Só usa a biblioteca padrão (roda em qualquer Python 3.8+, sem pip).
O token NUNCA fica neste arquivo: vem da variável de ambiente BEECOOLER_TOKEN.

Exemplos
  # 1) só converter e mostrar o que seria enviado (não toca na rede)
  python beecooler_uplink.py build --gateway-log gateway_40min.log --node-log sensor_node_40min.log --boot 234

  # 2) enviar de verdade
  export BEECOOLER_TOKEN='...'
  python beecooler_uplink.py send --gateway-log gateway_40min.log --boot 234 --gateway-id GW01

  # 3) sondar rotas e respostas de erro (não grava nada válido)
  python beecooler_uplink.py probe --gateway-id GW01
"""
import argparse, base64, json, os, re, struct, sys, time, urllib.request, urllib.error

API = os.environ.get("BEECOOLER_API", "https://t8n9fc4egf.execute-api.us-east-2.amazonaws.com/v1")

# ---------------------------------------------------------------- registro canônico v1 (Arquitetura v1.2 §5.2)
REC_V1 = struct.Struct('<IhHhHHHH4BH')
assert REC_V1.size == 24
I16_NA, U16_NA, U8_NA = -32768, 0xFFFF, 0xFF
FIELDS = ("ts", "t_in", "rh_in", "t_out", "rh_out", "vbat_mv", "vib_rms", "vib_peak")


def pack_v1(r: dict) -> bytes:
    b = r["bands"]
    return REC_V1.pack(r["ts"], r["t_in"], r["rh_in"], r["t_out"], r["rh_out"], r["vbat_mv"],
                       r["vib_rms"], r["vib_peak"], b[0], b[1], b[2], b[3], r["flags"])


def _v(x, na, k):
    return None if x == na else round(x * k, 4)


def decode_v1(buf: bytes, seq: int) -> dict:
    """Mesmo decodificador da Integração v1.1 §7.1 (para conferência local)."""
    (ts, t_in, rh_in, t_out, rh_out, vbat, rms, peak, b0, b1, b2, b3, flags) = REC_V1.unpack(buf)
    return {
        'seq': seq, 'ts': ts, 'flags': flags, 'power_level': (flags >> 9) & 0b11,
        't_in': _v(t_in, I16_NA, 0.01), 'rh_in': _v(rh_in, U16_NA, 0.01),
        't_out': _v(t_out, I16_NA, 0.01), 'rh_out': _v(rh_out, U16_NA, 0.01),
        'vbat': _v(vbat, U16_NA, 0.001),
        'vib_rms': _v(rms, U16_NA, 0.1), 'vib_peak': _v(peak, U16_NA, 0.1),
        'band_db': [None if b == U8_NA else b / 2 - 100 for b in (b0, b1, b2, b3)],
    }

# ---------------------------------------------------------------- parsers dos logs
RX_RE = re.compile(r"\[LoRa RX\] OK:.*no=(\d+) boot=(\d+) quadro=(\d+) tentativa=(\d+)")
NODE_RE = re.compile(r"^\[rec\] seq=(\d+) ts=(\d+) t_in=(-?\d+) rh_in=(\d+) t_out=(-?\d+) rh_out=(\d+) "
                     r"vbat=(\d+) rms=(\d+) peak=(\d+) bands=(\d+),(\d+),(\d+),(\d+) flags=0x([0-9A-Fa-f]+)")


def parse_gateway_log(path):
    """Lê as linhas BEEREC {...} da central; o boot_id vem da linha [LoRa RX] anterior."""
    out, boot = [], None
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = RX_RE.search(line)
            if m:
                boot = int(m.group(2))
                continue
            if line.startswith("BEEREC "):
                j = json.loads(line[7:])
                j["boot_id"] = boot
                out.append(j)
    return out


def parse_node_log(path):
    out = []
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = NODE_RE.match(line.strip())
            if m:
                g = m.groups()
                out.append({"seq": int(g[0]), "ts": int(g[1]), "t_in": int(g[2]), "rh_in": int(g[3]),
                            "t_out": int(g[4]), "rh_out": int(g[5]), "vbat_mv": int(g[6]),
                            "vib_rms": int(g[7]), "vib_peak": int(g[8]),
                            "bands": [int(x) for x in g[9:13]], "flags": int(g[13], 16)})
    return out

# ---------------------------------------------------------------- montagem do corpo §5.2

def runs_of(recs):
    """Agrupa registros (ordenados por seq) em trechos de seq contíguos."""
    runs, cur = [], []
    for r in sorted(recs, key=lambda r: r["seq"]):
        if cur and r["seq"] != cur[-1]["seq"] + 1:
            runs.append(cur); cur = []
        cur.append(r)
    if cur:
        runs.append(cur)
    return [{"first_seq": c[0]["seq"], "rec_ver": 1,
             "data": base64.b64encode(b"".join(pack_v1(r) for r in c)).decode()} for c in runs]


def build_body(recs, gateway_id, upload_id, node_override=None, include_rssi=False):
    now = int(time.time())
    sessions = {}
    for r in recs:
        key = (node_override or r["node"], r["boot_id"])
        sessions.setdefault(key, []).append(r)
    out = []
    for (node, boot), rs in sorted(sessions.items(), key=lambda kv: kv[0][1] or 0):
        valid_ts = [r["ts"] for r in rs if r["ts"] <= now]
        s = {"node_id": node, "boot_id": boot,
             # A central de bancada não tem NTP: usamos o maior ts plausível como aproximação.
             "rx_time": max(valid_ts) if valid_ts else now,
             "attempts": [r.get("attempt", 1) for r in sorted(rs, key=lambda r: r["seq"])],
             "runs": runs_of(rs)}
        if include_rssi:
            s["rssi"], s["snr"] = None, None   # DRSSI=0 no DX-LR32: sem RSSI/SNR por quadro
        out.append(s)
    return {"v": 1, "gateway_id": gateway_id, "upload_id": upload_id, "gw_time": now, "sessions": out}

# ---------------------------------------------------------------- HTTP

def http(method, path, body=None, token=None, raw=None, timeout=20):
    data = raw if raw is not None else (json.dumps(body).encode() if body is not None else None)
    req = urllib.request.Request(API + path, data=data, method=method)
    req.add_header("Content-Type", "application/json")
    if token:
        req.add_header("Authorization", "Bearer " + token)
    t0 = time.time()
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            code, text = resp.status, resp.read().decode(errors="replace")
    except urllib.error.HTTPError as e:
        code, text = e.code, e.read().decode(errors="replace")
    except Exception as e:  # rede, DNS, TLS
        code, text = None, repr(e)
    return code, text, round((time.time() - t0) * 1000)

# ---------------------------------------------------------------- comandos

def select(args):
    recs = parse_gateway_log(args.gateway_log)
    if args.boot:
        recs = [r for r in recs if r["boot_id"] in args.boot]
    return recs


def report(recs, node_recs):
    print(f"registros da central selecionados: {len(recs)}")
    by_boot = {}
    for r in recs:
        by_boot.setdefault(r["boot_id"], []).append(r["seq"])
    for b, seqs in by_boot.items():
        print(f"  boot {b}: seq {min(seqs)}..{max(seqs)} ({len(seqs)} registros)")
    seen, coll = {}, set()
    for r in recs:
        k = (r["node"], r["seq"])
        if k in seen and seen[k] != r["boot_id"]:
            coll.add(r["seq"])
        seen[k] = r["boot_id"]
    if coll:
        print(f"  ATENÇÃO: seq repetido entre boots (vira 'conflicts' na nuvem): {sorted(coll)}")
    now = time.time()
    bad_ts = [r["seq"] for r in recs if r["ts"] > now]
    if bad_ts:
        print(f"  ATENÇÃO: ts no futuro (RTC não validado, flag rtc_invalid=0): seq {bad_ts}")
    low_v = [r["seq"] for r in recs if not 2500 <= r["vbat_mv"] <= 4500]
    if low_v:
        print(f"  ATENÇÃO: vbat fora de 2,5–4,5 V em {len(low_v)} registros → cloud_range_fail")
    # conferência byte a byte com o log do nó
    if node_recs is not None:
        gw = {r["seq"]: r for r in recs}
        diff = 0
        for n in node_recs:
            g = gw.get(n["seq"])
            if g and pack_v1(g) != pack_v1(n):
                diff += 1
                print(f"  DIFERENÇA nó×central no seq {n['seq']}")
        print(f"  conferência nó × central: {len(node_recs)} registros do nó, {diff} diferenças")
    # ida e volta pelo decodificador
    for r in recs[:1]:
        print("  exemplo decodificado:", json.dumps(decode_v1(pack_v1(r), r["seq"]), ensure_ascii=False))


def cmd_build(args):
    recs = select(args)
    node_recs = parse_node_log(args.node_log) if args.node_log else None
    report(recs, node_recs)
    body = build_body(recs, args.gateway_id, args.upload_id, args.node_id, args.rssi)
    js = json.dumps(body, indent=2)
    print(f"corpo: {len(json.dumps(body))} B (limite 32 KB)")
    if args.out:
        open(args.out, "w").write(js)
        print("salvo em", args.out)
    else:
        print(js)


def cmd_send(args):
    tok = os.environ.get("BEECOOLER_TOKEN")
    if not tok:
        sys.exit("defina BEECOOLER_TOKEN no ambiente")
    recs = select(args)
    body = build_body(recs, args.gateway_id, args.upload_id, args.node_id, args.rssi)
    code, text, ms = http("POST", "/telemetry", body, tok)
    print(f"POST /telemetry → {code} em {ms} ms\n{text}")


def cmd_probe(args):
    tok = os.environ.get("BEECOOLER_TOKEN")
    gid = args.gateway_id
    ok_body = {"v": 1, "gateway_id": gid, "upload_id": 0, "gw_time": int(time.time()), "sessions": []}
    tests = [
        ("POST", "/telemetry", ok_body, None, None, "sem token (esperado 401)"),
        ("POST", "/telemetry", ok_body, "token-errado", None, "token errado (esperado 401)"),
    ]
    if tok:
        bad23 = dict(ok_body, sessions=[{"node_id": 254, "boot_id": 0, "rx_time": int(time.time()), "attempts": [1],
                                          "runs": [{"first_seq": 1, "rec_ver": 1,
                                                    "data": base64.b64encode(b"\0" * 23).decode()}]}])
        badver = json.loads(json.dumps(bad23))
        badver["sessions"][0]["runs"][0]["data"] = base64.b64encode(b"\0" * 24).decode()
        badver["sessions"][0]["runs"][0]["rec_ver"] = 9
        tests += [
            ("POST", "/telemetry", None, tok, b"{nao e json", "JSON quebrado (esperado 400)"),
            ("POST", "/telemetry", bad23, tok, None, "trecho de 23 B (esperado 400)"),
            ("POST", "/telemetry", badver, tok, None, "rec_ver=9 (esperado 400)"),
            ("POST", "/telemetry", ok_body, tok, None, "corpo válido sem sessões (esperado 200 ou 400)"),
        ]
    tests += [("GET", p, None, None, None, "leitura pública") for p in
              ("/nodes", "/nodes/1/status", f"/gateways/{gid}/status")]
    for m, p, b, t, raw, label in tests:
        code, text, ms = http(m, p, b, t, raw)
        print(f"{m:4} {p:28} {label:42} → {code} ({ms} ms) {text[:300]}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    for name in ("build", "send"):
        p = sub.add_parser(name)
        p.add_argument("--gateway-log", required=True)
        p.add_argument("--node-log")
        p.add_argument("--boot", type=int, action="append", help="filtra por boot_id (repetível)")
        p.add_argument("--gateway-id", default="GW01")
        p.add_argument("--upload-id", type=int, default=int(time.time()) % 100000)
        p.add_argument("--node-id", type=int, help="força outro node_id (ex.: 254 para testes)")
        p.add_argument("--rssi", action="store_true", help="inclui rssi/snr = null")
        p.add_argument("--out")
    p = sub.add_parser("probe")
    p.add_argument("--gateway-id", default="GW01")
    args = ap.parse_args()
    {"build": cmd_build, "send": cmd_send, "probe": cmd_probe}[args.cmd](args)


if __name__ == "__main__":
    main()
