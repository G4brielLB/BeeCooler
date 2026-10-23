// Configuração do painel. As rotas de leitura (GET) são públicas (doc. de nuvem, seção 5),
// então nada aqui é segredo.
window.BEE_CONFIG = {
  // URL do stage do API Gateway, sem o /v1 no final.
  // Ex.: 'https://abc123.execute-api.us-east-1.amazonaws.com'
  API_BASE: 'https://t8n9fc4egf.execute-api.us-east-2.amazonaws.com',
  NODE_ID: 1,             // nó exibido; se GET /v1/nodes ainda não o listar, as leituras são buscadas mesmo assim
  GATEWAY_ID: 'GW01',     // usado se GET /v1/nodes não informar o gateway do nó
  REFRESH_S: 60,          // consulta a GET /v1/nodes (D61)
  INITIAL_DAYS: 30,       // histórico carregado ao abrir; períodos mais antigos são buscados sob demanda
  GAP_S: 1800,            // intervalo sem dados acima do qual a linha do gráfico é quebrada (ciclo de 10 min × 3)
  NODE_SILENT_MIN: 70,    // RF-11: nó silencioso
  GW_SILENT_MIN: 30,      // RF-11: central sem contato (2 heartbeats perdidos)
  IDEAL_T_IN: [32, 36],   // faixa ideal da temperatura interna (a validar)
  IDEAL_RH_IN: [50, 75],  // faixa ideal da umidade interna (a validar)
};
