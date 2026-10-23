// Cliente das rotas de leitura da API BeeCooler (docs/BeeCooler — Integração em Nuvem v1.1, seção 5.1).
// Cada resposta é normalizada aqui, para o resto do painel não depender do formato exato do JSON.
// Os pontos marcados com CONFERIR seguem a documentação, mas dependem do que a Lambda devolver.
const API = (() => {
  const C = window.BEE_CONFIG;
  let mock = null;

  async function get(path, q) {
    if (mock) return mock(path, q || {});
    const qs = q ? '?' + new URLSearchParams(Object.entries(q).filter(([, v]) => v != null)) : '';
    const base = C.API_BASE.replace(/\/+$/, '').replace(/\/v1$/, '');
    const r = await fetch(base + path + qs, { headers: { Accept: 'application/json' } });
    if (r.status === 404) return null;
    if (!r.ok) throw new Error(`HTTP ${r.status} em ${path}`);
    return r.json();
  }

  // Números do DynamoDB (Decimal) podem chegar como string; ausência vira null (sentinela = lacuna).
  const num = x => (x == null || x === '' ? null : Number.isFinite(+x) ? +x : null);
  // Epoch em segundos; aceita milissegundos e ISO-8601.
  const epoch = x => {
    if (x == null || x === '') return null;
    const n = +x;
    if (Number.isFinite(n)) return n > 1e12 ? Math.floor(n / 1e3) : n;
    const p = Date.parse(x);
    return Number.isNaN(p) ? null : Math.floor(p / 1e3);
  };
  const pick = (o, ...ks) => { for (const k of ks) if (o && o[k] != null) return o[k]; return null; };
  // CONFERIR: lista em `items` (ou array puro) e próxima página em `next`.
  const list = b => (Array.isArray(b) ? b : pick(b, 'items', 'Items', 'readings', 'nodes') || []);
  const nextOf = b => {
    const n = pick(b, 'next', 'next_token', 'nextToken', 'LastEvaluatedKey');
    return n == null ? null : typeof n === 'string' ? n : JSON.stringify(n);
  };
  // Histograma de tentativas: [n1, n2, n3, n4] ou {"1": n1, ...}.
  const hist = h => {
    if (!h) return null;
    if (Array.isArray(h)) return h.map(v => num(v) || 0);
    return [1, 2, 3, 4].map(i => num(h[i]) || 0);
  };

  // Linha usada pelo painel: [ts, ti, ri, to, ro, rms, pk, b0, b1, b2, b3, vb, seq, flags]
  function reading(it) {
    const b = pick(it, 'band_db') || [];
    return [epoch(it.ts), num(it.t_in), num(it.rh_in), num(it.t_out), num(it.rh_out),
      num(it.vib_rms), num(it.vib_peak), num(b[0]), num(b[1]), num(b[2]), num(b[3]),
      num(it.vbat), num(it.seq), num(it.flags)];
  }

  return {
    async useMock() {
      await new Promise((ok, err) => {
        const s = document.createElement('script');
        s.src = 'js/mock.js'; s.onload = ok; s.onerror = () => err(new Error('js/mock.js não encontrado'));
        document.head.appendChild(s);
      });
      mock = window.BEE_MOCK;
    },
    get isMock() { return !!mock; },

    // GET /v1/nodes — CONFERIR nomes: node_id, max_seq, last_contact, gateway_id.
    async nodes() {
      return list(await get('/v1/nodes')).map(n => ({
        node_id: num(pick(n, 'node_id', 'id')),
        max_seq: num(n.max_seq),
        last_contact: epoch(pick(n, 'last_contact', 'last_rx', 'rx_time')),
        gateway_id: pick(n, 'gateway_id'),
      }));
    },

    // GET /v1/nodes/{id}/readings?from=&to= — segue a paginação até o fim.
    async readings(id, from, to) {
      const out = [];
      let next = null;
      for (let p = 0; p < 500; p++) {
        const b = await get(`/v1/nodes/${id}/readings`, { from, to, next });
        out.push(...list(b).map(reading).filter(r => r[0] != null));
        next = nextOf(b);
        if (!next) break;
      }
      return out;
    },

    // GET /v1/nodes/{id}/status — último STATUS decodificado + estado do enlace (seção 6.2).
    async status(id) {
      const s = await get(`/v1/nodes/${id}/status`);
      if (!s) return null;
      return {
        max_seq: num(s.max_seq),
        last_contact: epoch(pick(s, 'last_contact', 'last_rx', 'rx_time')),
        rssi: num(pick(s, 'rssi', 'rssi_ack')),
        snr: num(s.snr),
        backlog: num(s.backlog),
        power_level: num(s.power_level),
        attempts: hist(pick(s, 'attempt_hist', 'attempts_hist', 'attempts')),
        sd_free_mb: num(s.sd_free_mb),
        sd_errors: num(s.sd_errors),
        reset_reason: pick(s, 'reset_reason'),
        failed_sessions: num(s.failed_sessions),
        vbat_load: num(s.vbat_load),
        fw_version: pick(s, 'fw_version', 'fw'),
      };
    },

    // GET /v1/gateways/{id}/status — último heartbeat (seção 4.3).
    async gateway(id) {
      const h = await get(`/v1/gateways/${id}/status`);
      if (!h) return null;
      return {
        time: epoch(pick(h, 'received_at', 'ts', 'gw_time', 'time')),
        uptime_s: num(pick(h, 'uptime_s', 'uptime')),
        free_heap: num(pick(h, 'free_heap', 'heap_free', 'heap')),
        wifi_rssi: num(pick(h, 'wifi_rssi', 'rssi')),
        queue: num(pick(h, 'queue', 'queue_len', 'queue_size')),
        quarantine: num(pick(h, 'quarantine', 'quarantined')),
        ntp_ok: pick(h, 'ntp_ok', 'ntp_valid', 'ntp'),
        fw_version: pick(h, 'fw_version', 'fw'),
      };
    },
  };
})();
