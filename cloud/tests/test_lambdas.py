"""Teste de ponta a ponta das Lambdas contra um DynamoDB simulado (moto).

    pip install boto3 "moto[dynamodb]"
    python -m unittest discover -s cloud/tests
"""
import base64
import importlib.util
import json
import os
import struct
import sys
import time
import unittest

from moto import mock_aws

HERE = os.path.dirname(os.path.abspath(__file__))
CLOUD = os.path.dirname(HERE)
os.environ.update(AWS_DEFAULT_REGION="us-east-2", AWS_REGION="us-east-2",
                  AWS_ACCESS_KEY_ID="test", AWS_SECRET_ACCESS_KEY="test",
                  GW_TOKEN_GW01="tok")

REC = struct.Struct('<IhHhHHHH4BH')


def load(name, path):
    sys.path.insert(0, os.path.dirname(path))
    try:
        spec = importlib.util.spec_from_file_location(name, path)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        return mod
    finally:
        sys.path.pop(0)


def record(ts, t_in=2819, flags=0):
    return REC.pack(ts, t_in, 4423, 2801, 4674, 810, 343, 2773, 239, 245, 248, 251, flags)


def status_frame(ts):
    tail = struct.pack('<HHBBBBHH', 3, 1200, 0, 8, 71, 0, 3950, 0x0100)
    return record(ts) + tail


class LambdaTest(unittest.TestCase):
    def setUp(self):
        self.mock = mock_aws()
        self.mock.start()
        import boto3
        db = boto3.client("dynamodb", region_name="us-east-2")
        db.create_table(
            TableName="BeeCoolerReadings", BillingMode="PAY_PER_REQUEST",
            KeySchema=[{"AttributeName": "node_id", "KeyType": "HASH"},
                       {"AttributeName": "seq", "KeyType": "RANGE"}],
            AttributeDefinitions=[{"AttributeName": n, "AttributeType": "N"} for n in ("node_id", "seq", "ts")],
            GlobalSecondaryIndexes=[{
                "IndexName": "by_ts",
                "KeySchema": [{"AttributeName": "node_id", "KeyType": "HASH"},
                              {"AttributeName": "ts", "KeyType": "RANGE"}],
                "Projection": {"ProjectionType": "ALL"}}])
        db.create_table(
            TableName="BeeCoolerState", BillingMode="PAY_PER_REQUEST",
            KeySchema=[{"AttributeName": "PK", "KeyType": "HASH"},
                       {"AttributeName": "SK", "KeyType": "RANGE"}],
            AttributeDefinitions=[{"AttributeName": "PK", "AttributeType": "S"},
                                  {"AttributeName": "SK", "AttributeType": "S"}])
        sys.modules.pop("record", None)
        self.ingest = load("ingest_fn", os.path.join(CLOUD, "ingest", "lambda_function.py"))
        self.read = load("read_fn", os.path.join(CLOUD, "read_api", "lambda_function.py"))
        self.read.PAGE_SIZE = 4  # força paginação
        self.t0 = int(time.time()) - 3600

    def tearDown(self):
        self.mock.stop()

    def post(self, path, body):
        return self.ingest.lambda_handler({
            "rawPath": path, "requestContext": {"http": {"method": "POST"}},
            "headers": {"authorization": "Bearer tok"}, "body": json.dumps(body)}, None)

    def get(self, route, path_params=None, query=None):
        r = self.read.lambda_handler({
            "routeKey": route, "requestContext": {"http": {"method": "GET"}},
            "pathParameters": path_params, "queryStringParameters": query}, None)
        return r["statusCode"], json.loads(r["body"])

    def upload(self, first_seq, n, attempts, status=True, rx_offset=0):
        data = b"".join(record(self.t0 + 60 * (first_seq + i), t_in=-32768 if first_seq + i == 3 else 2819,
                               flags=1 if first_seq + i == 3 else 0) for i in range(n))
        return self.post("/v1/telemetry", {
            "v": 1, "gateway_id": "GW01", "upload_id": 1, "sessions": [{
                "node_id": 1, "boot_id": 7, "rx_time": self.t0 + 60 * (first_seq + n) + rx_offset,
                "rssi": 0, "snr": 0, "attempts": attempts,
                "status": base64.b64encode(status_frame(self.t0)).decode() if status else None,
                "runs": [{"first_seq": first_seq, "rec_ver": 1, "data": base64.b64encode(data).decode()}]}]})

    def test_fluxo_completo(self):
        r = self.upload(1, 10, [1] * 9 + [2])
        self.assertEqual(json.loads(r["body"])["stored"], 10)
        # Reenvio do mesmo lote: duplicados não contam tentativa nem regridem estado.
        r = self.upload(1, 10, [1] * 10, rx_offset=-600)
        self.assertEqual(json.loads(r["body"])["duplicates"], 10)

        code, body = self.get("GET /v1/nodes")
        self.assertEqual(code, 200)
        self.assertEqual(body["nodes"], [{"node_id": 1, "max_seq": 10,
                                          "last_contact": self.t0 + 660, "gateway_id": "GW01"}])

        # Paginação: 10 itens em páginas de 4.
        items, nxt = [], None
        while True:
            code, body = self.get("GET /v1/nodes/{node_id}/readings", {"node_id": "1"},
                                  {"from": str(self.t0), "to": str(self.t0 + 3600), "next": nxt})
            self.assertEqual(code, 200)
            items += body["items"]
            nxt = body["next"]
            if not nxt:
                break
        self.assertEqual([i["seq"] for i in items], list(range(1, 11)))
        self.assertNotIn("raw", items[0])
        self.assertEqual(items[0]["t_in"], 28.19)
        self.assertEqual(items[0]["band_db"], [19.5, 22.5, 24, 25.5])
        self.assertIsNone(items[2]["t_in"])  # sentinela vira null
        self.assertEqual(items[2]["flags"], 1)

        code, st = self.get("GET /v1/nodes/{node_id}/status", {"node_id": "1"})
        self.assertEqual(code, 200)
        self.assertEqual(st["attempt_hist"], [9, 1, 0, 0])
        self.assertEqual((st["backlog"], st["sd_free_mb"], st["rssi"], st["vbat_load"]), (3, 1200, -71, 3.95))
        self.assertEqual((st["reset_reason"], st["fw_version"], st["snr"]), ("deep sleep", "1.0", None))

    def test_heartbeat(self):
        self.assertEqual(self.get("GET /v1/gateways/{gateway_id}/status", {"gateway_id": "GW01"})[0], 404)
        self.post("/v1/gateway/heartbeat", {"gateway_id": "GW01", "uptime_s": 120, "heap_free": 180000,
                                            "wifi_rssi": -60, "queue": 2, "quarantine": 0, "fw": 256,
                                            "ntp_valid": True})
        code, hb = self.get("GET /v1/gateways/{gateway_id}/status", {"gateway_id": "GW01"})
        self.assertEqual(code, 200)
        self.assertAlmostEqual(hb["received_at"], time.time(), delta=5)
        self.assertEqual((hb["uptime_s"], hb["free_heap"], hb["queue"], hb["fw_version"], hb["ntp_ok"]),
                         (120, 180000, 2, "1.0", True))

    def test_entradas_invalidas(self):
        self.assertEqual(self.get("GET /v1/nodes/{node_id}/status", {"node_id": "abc"})[0], 400)
        self.assertEqual(self.get("GET /v1/nodes/{node_id}/status", {"node_id": "999"})[0], 400)
        self.assertEqual(self.get("GET /v1/nodes/{node_id}/readings", {"node_id": "1"}, {"next": "lixo"})[0], 400)
        forged = base64.urlsafe_b64encode(json.dumps({"node_id": 2, "seq": 1, "ts": 1}).encode()).decode()
        self.assertEqual(self.get("GET /v1/nodes/{node_id}/readings", {"node_id": "1"}, {"next": forged})[0], 400)
        r = self.post("/v1/telemetry", {"gateway_id": "GW01", "sessions": [{"node_id": 1, "runs": [
            {"first_seq": 1, "rec_ver": 1, "data": base64.b64encode(b"x" * 23).decode()}]}]})
        self.assertEqual(r["statusCode"], 400)


if __name__ == "__main__":
    unittest.main()
