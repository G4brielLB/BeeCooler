#!/usr/bin/env python3
"""Collect BeeCooler gateway serial output and serve a local test monitor."""

import argparse
import csv
import json
import math
import threading
import time
import uuid
from collections import deque
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

import serial


RECORD_FIELDS = (
    "session_id", "received_at", "batch_id", "sample_index", "sensor_ms",
    "elapsed_ms", "internal_temp_c", "internal_rh_pct", "external_temp_c",
    "external_rh_pct", "accel_x_g", "accel_y_g", "accel_z_g",
    "accel_magnitude_g",
)
BATCH_FIELDS = (
    "session_id", "received_at", "batch_id", "batch_start_ms", "bytes",
    "records", "interval_ms", "duplicates",
)
STATS_FIELDS = (
    "session_id", "received_at", "valid_data_packets", "unique_fragments",
    "duplicate_fragments", "malformed_packets", "crc_failures",
    "acks_transmitted", "completed_batches", "incomplete_batches_replaced",
    "sequence_gaps", "receive_queue_drops",
)
INTEGER_RECORD_KEYS = ("batch_id", "index", "sensor_ms", "elapsed_ms")
FLOAT_RECORD_KEYS = (
    "internal_temp_c", "internal_rh_pct", "external_temp_c",
    "external_rh_pct", "accel_x_g", "accel_y_g", "accel_z_g",
    "accel_magnitude_g",
)
INTEGER_BATCH_KEYS = (
    "batch_id", "batch_start_ms", "bytes", "records", "interval_ms",
    "duplicates",
)
INTEGER_STATS_KEYS = STATS_FIELDS[2:]
HISTORY_LIMIT = 300


def iso_now():
    return datetime.now().astimezone().isoformat(timespec="milliseconds")


def parse_key_values(line, prefix):
    if not line.startswith(prefix + " "):
        return None
    values = {}
    for token in line.split()[1:]:
        key, separator, value = token.partition("=")
        if separator:
            values[key] = value
    return values


def parse_record(line):
    values = parse_key_values(line, "RECORD")
    if values is None:
        return None
    try:
        parsed = {key: int(values[key]) for key in INTEGER_RECORD_KEYS}
        parsed.update({key: float(values[key]) for key in FLOAT_RECORD_KEYS})
    except (KeyError, ValueError):
        return None
    parsed["sample_index"] = parsed.pop("index")
    return parsed


def parse_batch_complete(line):
    values = parse_key_values(line, "BATCH_COMPLETE")
    if values is None:
        return None
    try:
        return {key: int(values[key]) for key in INTEGER_BATCH_KEYS}
    except (KeyError, ValueError):
        return None


def parse_gateway_stats(line):
    values = parse_key_values(line, "GATEWAY_STATS")
    if values is None:
        return None
    try:
        return {key: int(values[key]) for key in INTEGER_STATS_KEYS}
    except (KeyError, ValueError):
        return None


def json_safe(value):
    if isinstance(value, float) and not math.isfinite(value):
        return None
    if isinstance(value, dict):
        return {key: json_safe(item) for key, item in value.items()}
    if isinstance(value, (list, tuple, deque)):
        return [json_safe(item) for item in value]
    return value


class CsvSink:
    def __init__(self, path, fieldnames):
        path.parent.mkdir(parents=True, exist_ok=True)
        needs_header = not path.exists() or path.stat().st_size == 0
        self.file = path.open("a", newline="", encoding="utf-8")
        self.writer = csv.DictWriter(self.file, fieldnames=fieldnames)
        if needs_header:
            self.writer.writeheader()
            self.file.flush()

    def append(self, row):
        self.writer.writerow(row)
        self.file.flush()

    def close(self):
        self.file.close()


class CollectorState:
    def __init__(self, session_id, port):
        self.lock = threading.Lock()
        self.session_id = session_id
        self.port = port
        self.connected = False
        self.last_received_at = None
        self.last_message = None
        self.latest_batch_id = None
        self.latest_batch_duplicates = None
        self.communication = {key: 0 for key in INTEGER_STATS_KEYS}
        self.latest_reading = None
        self.history = deque(maxlen=HISTORY_LIMIT)

    def connection(self, connected):
        with self.lock:
            self.connected = connected

    def observe_line(self, line, received_at):
        with self.lock:
            self.last_received_at = received_at
            self.last_message = line

    def observe_record(self, record):
        with self.lock:
            self.latest_batch_id = record["batch_id"]
            self.latest_reading = record.copy()
            self.history.append(record.copy())

    def observe_batch(self, batch):
        with self.lock:
            self.latest_batch_id = batch["batch_id"]
            self.latest_batch_duplicates = batch["duplicates"]

    def observe_stats(self, stats):
        with self.lock:
            self.communication.update(
                {key: stats[key] for key in INTEGER_STATS_KEYS if key in stats}
            )

    def snapshot(self):
        with self.lock:
            result = {
                "session_id": self.session_id,
                "serial": {
                    "connected": self.connected,
                    "port": self.port,
                    "last_received_at": self.last_received_at,
                    "last_message": self.last_message,
                },
                "communication": {
                    **self.communication,
                    "latest_batch_id": self.latest_batch_id,
                    "latest_batch_duplicates": self.latest_batch_duplicates,
                },
                "latest_reading": self.latest_reading,
                "history": list(self.history),
                "timestamp_note": (
                    "sensor_ms is milliseconds since sensor boot and may reset "
                    "after a reboot; it is not calendar time."
                ),
            }
        return json_safe(result)


def convert_record_row(row):
    try:
        record = {
            "session_id": row["session_id"],
            "received_at": row["received_at"],
            "batch_id": int(row["batch_id"]),
            "sample_index": int(row["sample_index"]),
            "sensor_ms": int(row["sensor_ms"]),
            "elapsed_ms": int(row["elapsed_ms"]),
        }
        record.update({key: float(row[key]) for key in FLOAT_RECORD_KEYS})
        return record
    except (KeyError, TypeError, ValueError):
        return None


def convert_batch_row(row, index):
    try:
        batch = {
            "index": index,
            "session_id": row["session_id"],
            "received_at": row["received_at"],
        }
        batch.update({key: int(row[key]) for key in INTEGER_BATCH_KEYS})
        return batch
    except (KeyError, TypeError, ValueError):
        return None


def convert_stats_row(row):
    try:
        stats = {
            "session_id": row["session_id"],
            "received_at": row["received_at"],
        }
        stats.update({key: int(row[key]) for key in INTEGER_STATS_KEYS})
        return stats
    except (KeyError, TypeError, ValueError):
        return None


class DataRepository:
    """Read persisted collector CSVs for historical dashboard drill-down."""

    def __init__(self, data_dir, lock):
        self.data_dir = Path(data_dir)
        self.lock = lock

    @staticmethod
    def _read_rows(path, converter):
        if not path.exists():
            return []
        with path.open(newline="", encoding="utf-8") as csv_file:
            converted = []
            for index, row in enumerate(csv.DictReader(csv_file)):
                value = converter(row, index) if converter is convert_batch_row else converter(row)
                if value is not None:
                    converted.append(value)
            return converted

    def _all(self):
        batches = self._read_rows(self.data_dir / "batches.csv", convert_batch_row)
        records = self._read_rows(self.data_dir / "records.csv", convert_record_row)
        stats = self._read_rows(self.data_dir / "gateway_stats.csv", convert_stats_row)
        return batches, records, stats

    @staticmethod
    def _stats_for_batch(batch, stats):
        same_session = [row for row in stats if row["session_id"] == batch["session_id"]]
        current_index = next(
            (index for index, row in enumerate(same_session)
             if row["received_at"] >= batch["received_at"]),
            None,
        )
        if current_index is None:
            return None, None
        current = same_session[current_index]
        previous = same_session[current_index - 1] if current_index > 0 else None
        delta = None
        if previous is not None:
            delta = {}
            for key in INTEGER_STATS_KEYS:
                difference = current[key] - previous[key]
                delta[key] = difference if difference >= 0 else None
        return current, delta

    def list_batches(self):
        with self.lock:
            batches, _records, stats = self._all()
        result = []
        for batch in batches:
            snapshot, delta = self._stats_for_batch(batch, stats)
            result.append({**batch, "stats": snapshot, "stats_delta": delta})
        return json_safe(list(reversed(result)))

    def get_batch(self, index):
        with self.lock:
            batches, records, stats = self._all()
        if index < 0 or index >= len(batches):
            return None
        batch = batches[index]
        next_received_at = next(
            (candidate["received_at"] for candidate in batches[index + 1:]
             if candidate["session_id"] == batch["session_id"]),
            None,
        )
        selected_records = [
            record for record in records
            if record["session_id"] == batch["session_id"]
            and record["batch_id"] == batch["batch_id"]
            and record["received_at"] >= batch["received_at"]
            and (next_received_at is None or record["received_at"] < next_received_at)
        ]
        selected_records.sort(key=lambda record: (record["sensor_ms"], record["sample_index"]))
        snapshot, delta = self._stats_for_batch(batch, stats)
        return json_safe({
            "batch": batch,
            "records": selected_records,
            "stats": snapshot,
            "stats_delta": delta,
        })

    def recent_records(self, limit=HISTORY_LIMIT):
        with self.lock:
            records = self._read_rows(
                self.data_dir / "records.csv", convert_record_row
            )
        return records[-limit:]

    def latest_stats(self):
        with self.lock:
            stats = self._read_rows(
                self.data_dir / "gateway_stats.csv", convert_stats_row
            )
        return stats[-1] if stats else None


DASHBOARD_HTML = r"""<!doctype html>
<html lang="pt-BR"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>BeeCooler · Monitor de campo</title>
<style>
:root{font-family:Inter,ui-sans-serif,system-ui,sans-serif;color:#17211b;background:#f5f2e9;--ink:#17211b;--muted:#66736b;--line:#dce2da;--panel:#fffef9;--green:#275d45;--mint:#dcecdf;--amber:#e9a23b;--blue:#3975a8;--red:#c55742;--purple:#7656a5}*{box-sizing:border-box}body{margin:0}.shell{max-width:1380px;margin:auto;padding:24px}.top{display:flex;justify-content:space-between;gap:20px;align-items:flex-start;margin-bottom:18px}.brand{display:flex;gap:13px;align-items:center}.mark{width:46px;height:46px;border-radius:14px;background:var(--amber);display:grid;place-items:center;font-size:25px}.eyebrow{font-size:12px;letter-spacing:.13em;text-transform:uppercase;color:var(--muted)}h1{font-size:25px;margin:2px 0 0}h2{font-size:17px;margin:0}h3{font-size:14px;margin:0;color:var(--muted);font-weight:600}.status{display:flex;align-items:center;gap:8px;border:1px solid var(--line);padding:8px 12px;border-radius:99px;background:var(--panel);font-size:13px}.dot{width:9px;height:9px;border-radius:50%;background:#a3aaa5}.dot.on{background:#31a66c;box-shadow:0 0 0 4px #31a66c22}.panel{background:var(--panel);border:1px solid var(--line);border-radius:16px;padding:18px;margin:14px 0;box-shadow:0 7px 28px #24382d0a}.section-head{display:flex;justify-content:space-between;align-items:center;gap:12px;margin-bottom:14px}.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(145px,1fr));gap:10px}.card{border:1px solid var(--line);border-radius:12px;padding:12px;background:#fafbf7;min-width:0}.card.warn{background:#fff8ea}.label{font-size:11px;text-transform:uppercase;letter-spacing:.06em;color:var(--muted)}.value{font-size:22px;font-weight:650;margin-top:4px;overflow-wrap:anywhere}.sub{font-size:12px;color:var(--muted);margin-top:3px}.sensor-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:10px}.sensor-card{padding:15px;border-radius:14px;color:white;min-height:106px;display:flex;flex-direction:column;justify-content:space-between}.sensor-card.temp{background:linear-gradient(135deg,#bb513e,#e28a55)}.sensor-card.humidity{background:linear-gradient(135deg,#286d94,#4da7b2)}.sensor-card.accel{background:linear-gradient(135deg,#395e49,#6e9278)}.sensor-card.time{background:linear-gradient(135deg,#5b4776,#8a6baa)}.sensor-title{font-size:12px;opacity:.82;text-transform:uppercase;letter-spacing:.07em}.sensor-value{font-size:25px;font-weight:650}.pair{display:flex;justify-content:space-between;gap:12px}.pair small{display:block;font-size:11px;opacity:.75}.charts{display:grid;grid-template-columns:1fr;gap:16px}.chart-block{min-width:0}.chart-title{display:flex;align-items:center;gap:14px;margin-bottom:3px}.legend{display:flex;gap:13px;font-size:12px;color:var(--muted)}.swatch{width:9px;height:9px;border-radius:50%;display:inline-block;margin-right:5px}.chart-wrap{height:245px;position:relative}canvas{width:100%;height:100%;display:block}select,button{font:inherit;border:1px solid var(--line);border-radius:9px;background:white;color:var(--ink);padding:8px 10px}button{cursor:pointer}button:hover{background:#f2f5ef}.controls{display:flex;gap:8px;align-items:center;flex-wrap:wrap}.controls select{min-width:320px}.batch-layout{display:grid;grid-template-columns:1.15fr .85fr;gap:16px}.summary-table,.records-table{width:100%;border-collapse:collapse;font-size:13px}.summary-table th,.summary-table td,.records-table th,.records-table td{padding:8px;border-bottom:1px solid var(--line);text-align:right;white-space:nowrap}.summary-table th:first-child,.records-table th:first-child,.records-table td:first-child{text-align:left}.summary-table th{color:var(--muted);font-weight:500}.table-scroll{overflow:auto;max-height:360px;border:1px solid var(--line);border-radius:10px}.records-table thead{position:sticky;top:0;background:var(--panel)}.message{margin:12px 0 0;padding:10px;border-radius:9px;background:#f3f5f1;font:12px ui-monospace,SFMono-Regular,monospace;color:#526057;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.note{font-size:12px;color:var(--muted)}.empty{padding:30px;text-align:center;color:var(--muted)}@media(max-width:850px){.sensor-grid,.batch-layout{grid-template-columns:1fr 1fr}.controls select{min-width:200px;max-width:100%}}@media(max-width:560px){.shell{padding:14px}.top,.section-head{align-items:stretch;flex-direction:column}.sensor-grid,.batch-layout{grid-template-columns:1fr}.value{font-size:19px}}
</style></head><body><main class="shell">
<header class="top"><div class="brand"><div class="mark">⬡</div><div><div class="eyebrow">Integração ESP-NOW LR</div><h1>BeeCooler · Monitor de campo</h1></div></div><div class="status"><span id="connectionDot" class="dot"></span><span id="connectionText">Aguardando coletor</span></div></header>
<section class="panel"><div class="section-head"><div><h2>Comunicação</h2><div id="session" class="note"></div></div><div id="lastSeen" class="note"></div></div><div id="communication" class="cards"></div><div id="lastMessage" class="message">Nenhuma mensagem recebida.</div></section>
<section class="panel"><div class="section-head"><div><h2>Leitura mais recente</h2><div class="note">Valores ambientais e vibração recebidos do sensor</div></div></div><div id="latestSensors" class="sensor-grid"></div></section>
<section class="panel"><div class="section-head"><div><h2>Histórico recente</h2><div class="note">Até 300 amostras em memória · eixo X em tempo do sensor desde o boot</div></div></div><div class="charts"><div class="chart-block"><div class="chart-title"><h3>Temperatura · 20–45 °C</h3><div class="legend"><span><i class="swatch" style="background:#c55742"></i>Interna</span><span><i class="swatch" style="background:#3975a8"></i>Externa</span></div></div><div class="chart-wrap"><canvas id="recentTemperature"></canvas></div></div><div class="chart-block"><div class="chart-title"><h3>Umidade · 0–100 %RH</h3><div class="legend"><span><i class="swatch" style="background:#3975a8"></i>Interna</span><span><i class="swatch" style="background:#7656a5"></i>Externa</span></div></div><div class="chart-wrap"><canvas id="recentHumidity"></canvas></div></div><div class="chart-block"><div class="chart-title"><h3>Magnitude da aceleração</h3><div class="legend"><span><i class="swatch" style="background:#275d45"></i>|a|</span></div></div><div class="chart-wrap"><canvas id="recentAcceleration"></canvas></div></div></div></section>
<section class="panel"><div class="section-head"><div><h2>Explorador de batches</h2><div class="note">Selecione qualquer batch salvo, inclusive de sessões anteriores</div></div><div class="controls"><button id="previousBatch" aria-label="Batch anterior">←</button><select id="batchSelect" aria-label="Selecionar batch"></select><button id="nextBatch" aria-label="Próximo batch">→</button><button id="latestBatch">Mais recente</button></div></div><div id="batchContent" class="empty">Carregando batches salvos…</div></section>
</main><script>
const COLORS={red:'#c55742',blue:'#3975a8',purple:'#7656a5',green:'#275d45',grid:'#dce2da',text:'#66736b'};let selectedBatchIndex=null;let batches=[];let selectedDetail=null;let lastStatus=null;
const fmt=(v,d=2)=>Number.isFinite(v)?v.toFixed(d):'—';const duration=ms=>{if(!Number.isFinite(ms))return'—';const s=Math.round(ms/1000);return s<60?`${s}s`:`${Math.floor(s/60)}m ${s%60}s`};
function card(label,value,sub='',warn=false){return `<div class="card${warn?' warn':''}"><div class="label">${label}</div><div class="value">${value??'—'}</div>${sub?`<div class="sub">${sub}</div>`:''}</div>`}
function finiteValues(rows,key){return rows.map(r=>r[key]).filter(Number.isFinite)}
function drawChart(id,rows,series,yMin,yMax,ySuffix,xMode='sensor'){const canvas=document.getElementById(id);if(!canvas)return;const rect=canvas.getBoundingClientRect(),d=window.devicePixelRatio||1,w=Math.max(rect.width,300),h=Math.max(rect.height,180);canvas.width=w*d;canvas.height=h*d;const c=canvas.getContext('2d');c.scale(d,d);c.clearRect(0,0,w,h);const p={l:55,r:16,t:14,b:34},pw=w-p.l-p.r,ph=h-p.t-p.b;c.font='11px system-ui';c.lineWidth=1;c.strokeStyle=COLORS.grid;c.fillStyle=COLORS.text;c.textAlign='right';c.textBaseline='middle';for(let i=0;i<=5;i++){const v=yMin+(yMax-yMin)*i/5,y=p.t+ph-i*ph/5;c.beginPath();c.moveTo(p.l,y);c.lineTo(w-p.r,y);c.stroke();c.fillText(`${Number.isInteger(v)?v:v.toFixed(2)}${ySuffix}`,p.l-7,y)}if(!rows.length){c.textAlign='center';c.fillText('Sem dados',p.l+pw/2,p.t+ph/2);return}const xs=rows.map((r,i)=>xMode==='elapsed'?r.elapsed_ms:(Number.isFinite(r.sensor_ms)?r.sensor_ms:i));let xMin=Math.min(...xs),xMax=Math.max(...xs);if(xMin===xMax)xMax=xMin+1;c.textAlign='center';c.textBaseline='top';for(let i=0;i<=4;i++){const value=xMin+(xMax-xMin)*i/4,x=p.l+pw*i/4;c.fillText(xMode==='elapsed'?`${(value/1000).toFixed(0)}s`:duration(value),x,h-p.b+9)}for(const s of series){c.beginPath();c.strokeStyle=s.color;c.lineWidth=2;let active=false;rows.forEach((r,i)=>{const value=r[s.key];if(!Number.isFinite(value))return;const x=p.l+(xs[i]-xMin)*pw/(xMax-xMin),y=p.t+(yMax-value)*ph/(yMax-yMin);if(y<p.t||y>p.t+ph)return;active?c.lineTo(x,y):c.moveTo(x,y);active=true});c.stroke()}}
function accelerationMax(rows){const values=finiteValues(rows,'accel_magnitude_g');return Math.max(1.2,values.length?Math.ceil(Math.max(...values)*10)/10:1.2)}
function renderCharts(prefix,rows,xMode){drawChart(prefix+'Temperature',rows,[{key:'internal_temp_c',color:COLORS.red},{key:'external_temp_c',color:COLORS.blue}],20,45,'°',xMode);drawChart(prefix+'Humidity',rows,[{key:'internal_rh_pct',color:COLORS.blue},{key:'external_rh_pct',color:COLORS.purple}],0,100,'%',xMode);drawChart(prefix+'Acceleration',rows,[{key:'accel_magnitude_g',color:COLORS.green}],0,accelerationMax(rows),'g',xMode)}
function metricSummary(rows,key){const values=finiteValues(rows,key);if(!values.length)return['—','—','—'];return[fmt(Math.min(...values)),fmt(values.reduce((a,b)=>a+b,0)/values.length),fmt(Math.max(...values))]}
function renderLatest(status){const r=status.latest_reading||{};document.getElementById('latestSensors').innerHTML=`<div class="sensor-card temp"><div class="sensor-title">Temperatura</div><div class="pair"><div><small>Interna</small><div class="sensor-value">${fmt(r.internal_temp_c)}°</div></div><div><small>Externa</small><div class="sensor-value">${fmt(r.external_temp_c)}°</div></div></div></div><div class="sensor-card humidity"><div class="sensor-title">Umidade relativa</div><div class="pair"><div><small>Interna</small><div class="sensor-value">${fmt(r.internal_rh_pct,1)}%</div></div><div><small>Externa</small><div class="sensor-value">${fmt(r.external_rh_pct,1)}%</div></div></div></div><div class="sensor-card accel"><div class="sensor-title">Aceleração</div><div class="sensor-value">${fmt(r.accel_magnitude_g,4)} g</div><div class="sub" style="color:#ffffffbb">X ${fmt(r.accel_x_g,4)} · Y ${fmt(r.accel_y_g,4)} · Z ${fmt(r.accel_z_g,4)}</div></div><div class="sensor-card time"><div class="sensor-title">Tempo do sensor</div><div class="sensor-value">${duration(r.sensor_ms)}</div><div class="sub" style="color:#ffffffbb">sensor_ms ${r.sensor_ms??'—'} · batch ${r.batch_id??'—'}</div></div>`}
function renderStatus(s){lastStatus=s;const connected=s.serial.connected;document.getElementById('connectionDot').className='dot '+(connected?'on':'');document.getElementById('connectionText').textContent=connected?'Gateway conectado':'Gateway desconectado';document.getElementById('session').textContent=`Sessão atual: ${s.session_id} · porta ${s.serial.port}`;document.getElementById('lastSeen').textContent=s.serial.last_received_at?`Última linha: ${new Date(s.serial.last_received_at).toLocaleTimeString()}`:'Sem mensagens nesta sessão';document.getElementById('lastMessage').textContent=s.serial.last_message||'Nenhuma mensagem recebida nesta sessão.';const m=s.communication;document.getElementById('communication').innerHTML=card('Batches completos',m.completed_batches,'contador cumulativo do gateway')+card('Pacotes válidos',m.valid_data_packets)+card('Fragmentos únicos',m.unique_fragments)+card('ACKs enviados',m.acks_transmitted)+card('Duplicados',m.duplicate_fragments,`último batch: ${m.latest_batch_duplicates??'—'}`,m.duplicate_fragments>0)+card('Malformados',m.malformed_packets,'',m.malformed_packets>0)+card('Falhas CRC',m.crc_failures,'',m.crc_failures>0)+card('Lacunas de sequência',m.sequence_gaps,'',m.sequence_gaps>0)+card('Perdas na fila RX',m.receive_queue_drops,'',m.receive_queue_drops>0);renderLatest(s);renderCharts('recent',s.history,'sensor')}
function populateBatchSelect(){const select=document.getElementById('batchSelect'),previous=select.value;select.innerHTML='';for(const b of batches){const option=document.createElement('option');option.value=b.index;option.textContent=`Batch ${b.batch_id} · ${new Date(b.received_at).toLocaleString()} · ${b.records} amostras`;select.appendChild(option)}if(selectedBatchIndex===null&&batches.length)selectedBatchIndex=batches[0].index;if(batches.some(b=>b.index===selectedBatchIndex))select.value=selectedBatchIndex;else if(previous)select.value=previous}
async function selectBatch(index){selectedBatchIndex=Number(index);document.getElementById('batchSelect').value=selectedBatchIndex;selectedDetail=await fetch(`/api/batch?index=${selectedBatchIndex}`,{cache:'no-store'}).then(r=>{if(!r.ok)throw Error('Batch não encontrado');return r.json()});renderBatch(selectedDetail)}
function renderBatch(d){const b=d.batch,rows=d.records,delta=d.stats_delta||{},snapshot=d.stats||{};const metrics=[['Temperatura interna','internal_temp_c','°C'],['Temperatura externa','external_temp_c','°C'],['Umidade interna','internal_rh_pct','%'],['Umidade externa','external_rh_pct','%'],['Aceleração |a|','accel_magnitude_g','g']];const summary=metrics.map(([name,key,unit])=>{const v=metricSummary(rows,key);return `<tr><th>${name}</th><td>${v[0]} ${unit}</td><td>${v[1]} ${unit}</td><td>${v[2]} ${unit}</td></tr>`}).join('');const tableRows=rows.map(r=>`<tr><td>${r.sample_index}</td><td>${(r.elapsed_ms/1000).toFixed(1)}s</td><td>${r.sensor_ms}</td><td>${fmt(r.internal_temp_c)}</td><td>${fmt(r.external_temp_c)}</td><td>${fmt(r.internal_rh_pct,1)}</td><td>${fmt(r.external_rh_pct,1)}</td><td>${fmt(r.accel_magnitude_g,4)}</td></tr>`).join('');document.getElementById('batchContent').className='';document.getElementById('batchContent').innerHTML=`<div class="cards">${card('Batch',b.batch_id,`sessão ${b.session_id.slice(-8)}`)}${card('Início no sensor',duration(b.batch_start_ms),`${b.batch_start_ms} ms desde boot`)}${card('Amostras',`${rows.length}/${b.records}`,`${b.interval_ms} ms nominal`)}${card('Carga útil',`${b.bytes} bytes`)}${card('Duplicados do batch',b.duplicates,'',b.duplicates>0)}${card('Pacotes válidos Δ',delta.valid_data_packets??'—',`acumulado: ${snapshot.valid_data_packets??'—'}`)}${card('Fragmentos únicos Δ',delta.unique_fragments??'—',`acumulado: ${snapshot.unique_fragments??'—'}`)}${card('ACKs enviados Δ',delta.acks_transmitted??'—',`acumulado: ${snapshot.acks_transmitted??'—'}`)}${card('Erros CRC Δ',delta.crc_failures??'—','',delta.crc_failures>0)}</div><div class="batch-layout" style="margin-top:18px"><div><div class="chart-title"><h3>Temperatura no batch</h3><div class="legend"><span><i class="swatch" style="background:#c55742"></i>Interna</span><span><i class="swatch" style="background:#3975a8"></i>Externa</span></div></div><div class="chart-wrap"><canvas id="batchTemperature"></canvas></div><h3>Umidade no batch</h3><div class="chart-wrap"><canvas id="batchHumidity"></canvas></div><h3>Aceleração no batch</h3><div class="chart-wrap"><canvas id="batchAcceleration"></canvas></div></div><div><h3>Resumo dos sensores</h3><table class="summary-table"><thead><tr><th>Métrica</th><th>Mín.</th><th>Média</th><th>Máx.</th></tr></thead><tbody>${summary}</tbody></table><h3 style="margin-top:20px">Metadados</h3><table class="summary-table"><tbody><tr><th>Recebido no Mac</th><td>${new Date(b.received_at).toLocaleString()}</td></tr><tr><th>session_id</th><td>${b.session_id}</td></tr><tr><th>batch_start_ms</th><td>${b.batch_start_ms}</td></tr><tr><th>Estatística associada</th><td>${d.stats?'sim':'ainda não'}</td></tr></tbody></table></div></div><h3 style="margin:20px 0 8px">Amostras do batch</h3><div class="table-scroll"><table class="records-table"><thead><tr><th>#</th><th>Decorrido</th><th>sensor_ms</th><th>T int.</th><th>T ext.</th><th>UR int.</th><th>UR ext.</th><th>|a|</th></tr></thead><tbody>${tableRows}</tbody></table></div>`;renderCharts('batch',rows,'elapsed')}
async function refreshBatches(){const updated=await fetch('/api/batches',{cache:'no-store'}).then(r=>r.json());const newest=updated.length?updated[0].index:null;batches=updated;populateBatchSelect();if(!selectedDetail&&newest!==null)await selectBatch(newest);else if(selectedDetail&&selectedDetail.batch.index===newest)await selectBatch(newest)}
async function refresh(){try{const status=await fetch('/api/status',{cache:'no-store'}).then(r=>r.json());renderStatus(status);await refreshBatches()}catch(error){document.getElementById('connectionText').textContent='Monitor indisponível';document.getElementById('lastMessage').textContent=error}}
document.getElementById('batchSelect').addEventListener('change',event=>selectBatch(event.target.value));document.getElementById('latestBatch').addEventListener('click',()=>batches.length&&selectBatch(batches[0].index));document.getElementById('previousBatch').addEventListener('click',()=>{const i=batches.findIndex(b=>b.index===selectedBatchIndex);if(i<batches.length-1)selectBatch(batches[i+1].index)});document.getElementById('nextBatch').addEventListener('click',()=>{const i=batches.findIndex(b=>b.index===selectedBatchIndex);if(i>0)selectBatch(batches[i-1].index)});addEventListener('resize',()=>{if(lastStatus)renderCharts('recent',lastStatus.history,'sensor');if(selectedDetail)renderCharts('batch',selectedDetail.records,'elapsed')});refresh();setInterval(refresh,2000);
</script></body></html>"""


def make_handler(state, repository):
    class MonitorHandler(BaseHTTPRequestHandler):
        def do_GET(self):
            request = urlparse(self.path)
            if request.path == "/api/status":
                body = json.dumps(state.snapshot(), allow_nan=False).encode("utf-8")
                content_type = "application/json; charset=utf-8"
            elif request.path == "/api/batches":
                body = json.dumps(repository.list_batches(), allow_nan=False).encode("utf-8")
                content_type = "application/json; charset=utf-8"
            elif request.path == "/api/batch":
                try:
                    index = int(parse_qs(request.query)["index"][0])
                except (KeyError, IndexError, ValueError):
                    self.send_error(400, "A numeric batch index is required")
                    return
                detail = repository.get_batch(index)
                if detail is None:
                    self.send_error(404, "Batch not found")
                    return
                body = json.dumps(detail, allow_nan=False).encode("utf-8")
                content_type = "application/json; charset=utf-8"
            elif request.path in ("/", "/index.html"):
                body = DASHBOARD_HTML.encode("utf-8")
                content_type = "text/html; charset=utf-8"
            else:
                self.send_error(404)
                return
            self.send_response(200)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, _format, *_args):
            return

    return MonitorHandler


class GatewayCollector:
    def __init__(self, args):
        self.args = args
        self.session_id = (
            datetime.now().astimezone().strftime("%Y%m%dT%H%M%S%z")
            + "_" + uuid.uuid4().hex[:8]
        )
        self.data_dir = Path(args.data_dir)
        self.data_dir.mkdir(parents=True, exist_ok=True)
        self.storage_lock = threading.Lock()
        self.state = CollectorState(self.session_id, args.port)
        self.repository = DataRepository(self.data_dir, self.storage_lock)
        self.raw = (self.data_dir / f"gateway_raw_{self.session_id}.log").open(
            "a", encoding="utf-8", buffering=1
        )
        self.records = CsvSink(self.data_dir / "records.csv", RECORD_FIELDS)
        self.batches = CsvSink(self.data_dir / "batches.csv", BATCH_FIELDS)
        self.stats = CsvSink(self.data_dir / "gateway_stats.csv", STATS_FIELDS)
        for record in self.repository.recent_records():
            self.state.observe_record(record)
        saved_batches = self.repository.list_batches()
        if saved_batches:
            self.state.observe_batch(saved_batches[0])
        saved_stats = self.repository.latest_stats()
        if saved_stats:
            self.state.observe_stats(saved_stats)

    def process_line(self, line, received_at=None):
        received_at = received_at or iso_now()
        self.raw.write(f"{received_at} {line}\n")
        self.raw.flush()
        self.state.observe_line(line, received_at)

        record = parse_record(line)
        if record is not None:
            saved_record = {
                "session_id": self.session_id,
                "received_at": received_at,
                **record,
            }
            with self.storage_lock:
                self.records.append(saved_record)
            self.state.observe_record(saved_record)
            return

        batch = parse_batch_complete(line)
        if batch is not None:
            saved_batch = {
                "session_id": self.session_id,
                "received_at": received_at,
                **batch,
            }
            with self.storage_lock:
                self.batches.append(saved_batch)
            self.state.observe_batch(saved_batch)
            return

        stats = parse_gateway_stats(line)
        if stats is not None:
            saved_stats = {
                "session_id": self.session_id,
                "received_at": received_at,
                **stats,
            }
            with self.storage_lock:
                self.stats.append(saved_stats)
            self.state.observe_stats(saved_stats)

    def run(self):
        print(f"Collector session: {self.session_id}")
        print(f"Dashboard: http://{self.args.host}:{self.args.http_port}")
        print(f"Raw trace: {self.raw.name}")
        while True:
            try:
                with serial.Serial(self.args.port, self.args.baud, timeout=1) as port:
                    self.state.connection(True)
                    print(f"Gateway serial connected: {self.args.port}")
                    while True:
                        raw_line = port.readline()
                        if raw_line:
                            line = raw_line.decode("utf-8", errors="replace").rstrip("\r\n")
                            self.process_line(line)
            except (serial.SerialException, OSError) as error:
                self.state.connection(False)
                print(f"Gateway serial unavailable: {error}; retrying in 2 seconds")
                time.sleep(2)

    def close(self):
        self.raw.close()
        self.records.close()
        self.batches.close()
        self.stats.close()


def parse_args():
    project_data = Path(__file__).resolve().parents[1] / "data"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="Gateway USB serial port")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--http-port", type=int, default=8080)
    parser.add_argument("--data-dir", default=str(project_data))
    return parser.parse_args()


def main():
    args = parse_args()
    collector = GatewayCollector(args)
    server = ThreadingHTTPServer(
        (args.host, args.http_port),
        make_handler(collector.state, collector.repository),
    )
    server_thread = threading.Thread(target=server.serve_forever, daemon=True)
    server_thread.start()
    try:
        collector.run()
    except KeyboardInterrupt:
        print("\nStopping collector.")
    finally:
        collector.state.connection(False)
        server.shutdown()
        server.server_close()
        collector.close()


if __name__ == "__main__":
    main()
