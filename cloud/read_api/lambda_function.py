"""Rotas de leitura da API BeeCooler (docs/BeeCooler — Integração em Nuvem v1.1, seção 5.1).

GET /v1/nodes
GET /v1/nodes/{node_id}/readings?from=&to=&next=
GET /v1/nodes/{node_id}/status
GET /v1/gateways/{gateway_id}/status

Somente leitura e sem token: a proteção é o throttling do stage (seção 9).
"""
import base64
import json
import os
import time
from decimal import Decimal

import boto3

REGION = os.environ.get("AWS_REGION", "us-east-2")
READINGS_TABLE = os.environ.get("READINGS_TABLE", "BeeCoolerReadings")
STATE_TABLE = os.environ.get("STATE_TABLE", "BeeCoolerState")
TS_INDEX = os.environ.get("TS_INDEX", "by_ts")  # GSI: PK node_id, SK ts (seção 6.1)
ALLOWED_ORIGIN = os.environ.get("ALLOWED_ORIGIN", "*")

PAGE_SIZE = 1000
MAX_RANGE_S = 400 * 86400

dynamodb = boto3.resource("dynamodb", region_name=REGION)
readings_table = dynamodb.Table(READINGS_TABLE)
state_table = dynamodb.Table(STATE_TABLE)

# Campos devolvidos por leitura; `raw` (24 B) e metadados de ingestão ficam de fora.
READING_FIELDS = ["seq", "ts", "t_in", "rh_in", "t_out", "rh_out", "vbat",
                  "vib_rms", "vib_peak", "band_db", "flags", "power_level"]


class BadRequest(Exception):
    pass


def to_json(obj):
    if isinstance(obj, Decimal):
        return int(obj) if obj == obj.to_integral_value() else float(obj)
    if isinstance(obj, (bytes, bytearray)):
        return None
    raise TypeError(f"tipo não serializável: {type(obj).__name__}")


def response(status_code, body, max_age=0):
    headers = {
        "Content-Type": "application/json",
        "Access-Control-Allow-Origin": ALLOWED_ORIGIN,
        "Access-Control-Allow-Methods": "GET,OPTIONS",
        "Access-Control-Allow-Headers": "Content-Type",
    }
    if max_age:
        headers["Cache-Control"] = f"public, max-age={max_age}"
    return {"statusCode": status_code, "headers": headers,
            "body": json.dumps(body, default=to_json, ensure_ascii=False)}


def lambda_handler(event, context):
    ctx = event.get("requestContext", {})
    method = ctx.get("http", {}).get("method", event.get("httpMethod", ""))
    # routeKey ("GET /v1/nodes/{node_id}/status") não depende do nome do stage, ao contrário de rawPath.
    route = event.get("routeKey") or f"{method} {event.get('rawPath', event.get('path', ''))}"
    params = event.get("pathParameters") or {}
    query = event.get("queryStringParameters") or {}

    if method == "OPTIONS":
        return response(204, {})

    try:
        if route == "GET /v1/nodes":
            return response(200, list_nodes(), max_age=30)
        if route == "GET /v1/nodes/{node_id}/readings":
            return response(200, get_readings(parse_node_id(params), query), max_age=30)
        if route == "GET /v1/nodes/{node_id}/status":
            body = get_node_status(parse_node_id(params))
            return response(200, body, max_age=30) if body else response(404, {"error": "Nó sem STATUS"})
        if route == "GET /v1/gateways/{gateway_id}/status":
            body = get_gateway_status(params.get("gateway_id", ""))
            return response(200, body, max_age=30) if body else response(404, {"error": "Central sem heartbeat"})
    except BadRequest as e:
        return response(400, {"error": str(e)})

    return response(404, {"error": "Rota não encontrada"})


def parse_node_id(params):
    try:
        node_id = int(params.get("node_id", ""))
    except ValueError:
        raise BadRequest("node_id deve ser inteiro")
    if not 1 <= node_id <= 254:
        raise BadRequest("node_id fora de 1..254")
    return node_id


def parse_epoch(query, key, default):
    raw = query.get(key)
    if raw in (None, ""):
        return default
    try:
        return int(float(raw))
    except ValueError:
        raise BadRequest(f"{key} deve ser epoch em segundos")


def encode_next(key):
    return base64.urlsafe_b64encode(json.dumps(key, default=to_json).encode()).decode()


def decode_next(token, node_id):
    try:
        key = json.loads(base64.urlsafe_b64decode(token.encode()))
    except ValueError:
        raise BadRequest("next inválido")
    # O token só pode apontar para o mesmo nó e conter as chaves da tabela/índice.
    if not isinstance(key, dict) or key.get("node_id") != node_id or not set(key) <= {"node_id", "seq", "ts"}:
        raise BadRequest("next inválido")
    return {k: Decimal(str(v)) for k, v in key.items()}


def error_code(exc):
    return getattr(exc, "response", {}).get("Error", {}).get("Code", "")


# ---- GET /v1/nodes ------------------------------------------------------------

def list_nodes():
    items, kwargs = [], {
        "FilterExpression": "begins_with(PK, :p) AND SK = :s",
        "ExpressionAttributeValues": {":p": "NODE#", ":s": "STATUS"},
    }
    # A tabela de estado tem poucos itens (um por nó e por central), então o scan é barato.
    while True:
        page = state_table.scan(**kwargs)
        items.extend(page.get("Items", []))
        if "LastEvaluatedKey" not in page:
            break
        kwargs["ExclusiveStartKey"] = page["LastEvaluatedKey"]

    nodes = [{
        "node_id": int(it["PK"].split("#", 1)[1]),
        "max_seq": it.get("max_seq"),
        "last_contact": it.get("last_contact"),
        "gateway_id": it.get("gateway_id"),
    } for it in items]
    nodes.sort(key=lambda n: n["node_id"])
    return {"nodes": nodes}


# ---- GET /v1/nodes/{node_id}/readings -----------------------------------------

def get_readings(node_id, query):
    now = int(time.time())
    to = parse_epoch(query, "to", now)
    frm = parse_epoch(query, "from", to - 86400)
    if frm > to:
        frm, to = to, frm
    if to - frm > MAX_RANGE_S:
        raise BadRequest("intervalo maior que 400 dias")

    names = {f"#{f}": f for f in READING_FIELDS}
    names["#node"] = "node_id"
    kwargs = {
        "KeyConditionExpression": "#node = :n AND #ts BETWEEN :a AND :b",
        "ProjectionExpression": ", ".join(f"#{f}" for f in READING_FIELDS),
        "ExpressionAttributeNames": names,
        "ExpressionAttributeValues": {":n": node_id, ":a": frm, ":b": to},
        "Limit": PAGE_SIZE,
    }
    if query.get("next"):
        kwargs["ExclusiveStartKey"] = decode_next(query["next"], node_id)

    try:
        page = readings_table.query(IndexName=TS_INDEX, **kwargs)
    except Exception as e:
        if error_code(e) != "ValidationException":
            raise
        # Sem o GSI by_ts: lê a partição do nó por seq e filtra por ts. Funciona, mas
        # consome leitura de todo o histórico do nó; crie o índice (cloud/README.md).
        print(f"[readings] índice {TS_INDEX} indisponível, usando filtro: {e}")
        kwargs["KeyConditionExpression"] = "#node = :n"
        kwargs["FilterExpression"] = "#ts BETWEEN :a AND :b"
        page = readings_table.query(**kwargs)

    last = page.get("LastEvaluatedKey")
    return {"items": page.get("Items", []), "next": encode_next(last) if last else None}


# ---- GET /v1/nodes/{node_id}/status -------------------------------------------

def get_node_status(node_id):
    it = state_table.get_item(Key={"PK": f"NODE#{node_id}", "SK": "STATUS"}).get("Item")
    if not it:
        return None
    hist = [it.get(f"att_{i}", 0) for i in range(1, 5)]
    rssi = it.get("rssi_ack") or it.get("rx_rssi")
    return {
        "node_id": node_id,
        "max_seq": it.get("max_seq"),
        "last_contact": it.get("last_contact"),
        "gateway_id": it.get("gateway_id"),
        "rssi": rssi,
        "snr": it.get("rx_snr"),
        "backlog": it.get("backlog"),
        "power_level": it.get("power_level"),
        "attempt_hist": hist if any(hist) else None,
        "sd_free_mb": it.get("sd_free_mb"),
        "sd_errors": it.get("sd_errors"),
        "reset_reason": it.get("reset_reason"),
        "failed_sessions": it.get("failed_sessions"),
        "vbat_load": it.get("vbat_load"),
        "fw_version": it.get("fw_version"),
        "status_at": it.get("status_at"),
    }


# ---- GET /v1/gateways/{gateway_id}/status -------------------------------------

def get_gateway_status(gateway_id):
    if not gateway_id or len(gateway_id) > 32:
        raise BadRequest("gateway_id inválido")
    it = state_table.get_item(Key={"PK": f"GW#{gateway_id}", "SK": "HEARTBEAT"}).get("Item")
    if not it:
        return None
    return {
        "gateway_id": gateway_id,
        "received_at": it.get("received_at"),
        "uptime_s": it.get("uptime_s"),
        "free_heap": it.get("heap_free"),
        "wifi_rssi": it.get("wifi_rssi"),
        "queue": it.get("queue"),
        "quarantine": it.get("quarantine"),
        "ntp_ok": it.get("ntp_valid"),
        "uplink_suspended": it.get("uplink_suspended"),
        "fw_version": fw_text(it.get("fw")),
    }


def fw_text(v):
    """kFirmwareVersion vem como uint16 (0x0100 -> '1.0')."""
    if isinstance(v, Decimal):
        v = int(v)
        return f"{v >> 8}.{v & 0xFF}"
    return v
