import json
import base64
import os
import time
import boto3
from decimal import Decimal
from record import decode_v1, decode_status

# Força o uso da região us-east-2
dynamodb = boto3.resource('dynamodb', region_name='us-east-2')
readings_table = dynamodb.Table('BeeCoolerReadings')
state_table = dynamodb.Table('BeeCoolerState')

# O token vem só da variável de ambiente da Lambda; sem ela, todo POST é recusado (401).
GATEWAY_TOKENS = {
    "GW01": os.environ.get("GW_TOKEN_GW01")
}

def decimal_default(obj):
    if isinstance(obj, Decimal):
        return float(obj)
    raise TypeError

def response(status_code, body):
    return {
        "statusCode": status_code,
        "headers": {
            "Content-Type": "application/json",
            "Access-Control-Allow-Origin": "*",
            "Access-Control-Allow-Headers": "Content-Type, Authorization",
            "Access-Control-Allow-Methods": "OPTIONS,POST,GET"
        },
        "body": json.dumps(body, default=decimal_default)
    }

def lambda_handler(event, context):
    path = event.get("rawPath", event.get("path", ""))
    method = event.get("requestContext", {}).get("http", {}).get("method", event.get("httpMethod", ""))

    if method == "OPTIONS":
        return response(200, {})

    if path == "/v1/telemetry" and method == "POST":
        return handle_telemetry(event)

    if path == "/v1/gateway/heartbeat" and method == "POST":
        return handle_heartbeat(event)

    return response(404, {"error": "Rota não encontrada"})

def authenticate_gateway(headers, gateway_id):
    auth_header = headers.get("authorization", headers.get("Authorization", ""))
    expected_token = GATEWAY_TOKENS.get(gateway_id)
    if not expected_token or auth_header != f"Bearer {expected_token}":
        return False
    return True

def handle_telemetry(event):
    headers = event.get("headers", {})
    try:
        body = json.loads(event.get("body", "{}"))
    except Exception:
        return response(400, {"error": "JSON malformado"})

    gateway_id = body.get("gateway_id")
    if not gateway_id or not authenticate_gateway(headers, gateway_id):
        return response(401, {"error": "Token não autorizado"})

    # Valida tudo antes de gravar: um trecho inválido vai para a quarentena da central (Nuvem 5.3).
    for session in body.get("sessions", []):
        for run in session.get("runs", []):
            if run.get("rec_ver", 1) != 1:
                return response(400, {"error": f"rec_ver desconhecido: {run.get('rec_ver')}"})
            try:
                if len(base64.b64decode(run.get("data", ""))) % 24:
                    return response(400, {"error": "Trecho com tamanho que não é múltiplo de 24 B"})
            except Exception:
                return response(400, {"error": "Base64 inválido em runs[].data"})

    stored = 0
    duplicates = 0
    conflicts = 0

    for session in body.get("sessions", []):
        node_id = session.get("node_id")
        attempts = session.get("attempts", [])
        new_attempts = []
        max_seq = None
        k = 0  # attempts[k] corresponde ao k-ésimo registro da sessão, na ordem dos runs
        for run in session.get("runs", []):
            first_seq = run.get("first_seq")
            data_b64 = run.get("data")
            raw_bytes = base64.b64decode(data_b64)

            num_records = len(raw_bytes) // 24
            for i in range(num_records):
                chunk = raw_bytes[i*24 : (i+1)*24]
                seq = first_seq + i
                item = decode_v1(chunk, seq)
                item["node_id"] = node_id
                item["gateway_id"] = gateway_id
                item["rx_time"] = session.get("rx_time")
                max_seq = seq if max_seq is None else max(max_seq, seq)

                try:
                    readings_table.put_item(
                        Item=item,
                        ConditionExpression="attribute_not_exists(seq)"
                    )
                    stored += 1
                    # Só conta a tentativa de registros novos, para reenvios não inflarem o histograma.
                    if k < len(attempts):
                        new_attempts.append(attempts[k])
                except dynamodb.meta.client.exceptions.ConditionalCheckFailedException:
                    duplicates += 1
                k += 1

        update_node_state(node_id, gateway_id, session, max_seq, new_attempts)

    return response(200, {
        "upload_id": body.get("upload_id"),
        "stored": stored,
        "duplicates": duplicates,
        "conflicts": conflicts,
        "commands": []
    })

def update_node_state(node_id, gateway_id, session, max_seq, new_attempts):
    """Mantém o item NODE#<id>/STATUS lido por GET /v1/nodes e /v1/nodes/{id}/status (Nuvem 6.2)."""
    if node_id is None:
        return
    key = {"PK": f"NODE#{node_id}", "SK": "STATUS"}
    check_failed = dynamodb.meta.client.exceptions.ConditionalCheckFailedException
    rx_time = session.get("rx_time")

    sets = {"gateway_id": gateway_id}
    # O gateway manda 0 quando o módulo não mede RSSI/SNR (modo transparente).
    if session.get("rssi"):
        sets["rx_rssi"] = session["rssi"]
    if session.get("snr"):
        sets["rx_snr"] = session["snr"]
    if session.get("boot_id") is not None:
        sets["boot_id"] = session["boot_id"]
    if session.get("status"):
        try:
            st = decode_status(base64.b64decode(session["status"]))
            rec = st.pop("record")
            sets.update({k: v for k, v in st.items() if v is not None})
            sets["power_level"] = rec["power_level"]
            sets["status_at"] = rx_time
        except ValueError as e:
            print(f"[telemetry] STATUS ignorado do nó {node_id}: {e}")

    names = {f"#{k}": k for k in sets}
    values = {f":{k}": v for k, v in sets.items()}
    expr = "SET " + ", ".join(f"#{k} = :{k}" for k in sets)
    counts = {}
    for a in new_attempts:
        if isinstance(a, int) and 1 <= a <= 4:
            counts[a] = counts.get(a, 0) + 1
    if counts:
        expr += " ADD " + ", ".join(f"#att_{a} :att_{a}" for a in counts)
        names.update({f"#att_{a}": f"att_{a}" for a in counts})
        values.update({f":att_{a}": n for a, n in counts.items()})
    state_table.update_item(Key=key, UpdateExpression=expr,
                            ExpressionAttributeNames=names, ExpressionAttributeValues=values)

    # max_seq e last_contact só avançam: uma fila drenada fora de ordem não pode fazê-los voltar.
    for attr, value in (("max_seq", max_seq), ("last_contact", rx_time)):
        if value is None:
            continue
        try:
            state_table.update_item(
                Key=key,
                UpdateExpression="SET #a = :v",
                ConditionExpression="attribute_not_exists(#a) OR #a < :v",
                ExpressionAttributeNames={"#a": attr},
                ExpressionAttributeValues={":v": value},
            )
        except check_failed:
            pass

def handle_heartbeat(event):
    headers = event.get("headers", {})
    try:
        body = json.loads(event.get("body", "{}"))
    except Exception:
        return response(400, {"error": "JSON malformado"})

    gateway_id = body.get("gateway_id", "GW01")
    if not authenticate_gateway(headers, gateway_id):
        return response(401, {"error": "Token não autorizado"})

    # Nomes enviados por sendHeartbeat() em src/gateway/UplinkTask.cpp. received_at é a hora
    # da nuvem: é por ela que o painel decide "central sem contato" (30 min).
    state_table.put_item(
        Item={
            "PK": f"GW#{gateway_id}",
            "SK": "HEARTBEAT",
            "received_at": int(time.time()),
            "uptime_s": body.get("uptime_s"),
            "heap_free": body.get("heap_free"),
            "queue": body.get("queue"),
            "quarantine": body.get("quarantine"),
            "wifi_rssi": body.get("wifi_rssi"),
            "fw": body.get("fw"),
            "ntp_valid": body.get("ntp_valid", True),
            "uplink_suspended": body.get("uplink_suspended", False)
        }
    )
    return response(200, {"status": "ok"})
