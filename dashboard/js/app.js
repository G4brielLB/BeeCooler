// Lógica do painel. O desenho dos gráficos e os controles de período vêm do protótipo
// beecooler_painel_v6.html (especificação: BeeCooler_Dashboard_v5.md); os dados vêm da API (js/api.js).
const C=window.BEE_CONFIG,$=i=>document.getElementById(i),IN='#c2410c',OUT='#2563eb',TZ=-10800;
const NOW=()=>Math.floor(Date.now()/1e3);
const esc=s=>String(s).replace(/[&<>"']/g,c=>`&#${c.charCodeAt(0)};`);
const ico=n=>`<svg class="ic"><use href="#i-${n}"/></svg>`;
const fmt=(t,o)=>new Date(t*1e3).toLocaleString('pt-BR',{timeZone:'America/Sao_Paulo',...o});
const FF={day:'2-digit',month:'2-digit',hour:'2-digit',minute:'2-digit'},HMS={hour:'2-digit',minute:'2-digit',second:'2-digit'};
const f=(v,d)=>v==null?'—':v.toFixed(d),nn=a=>a.filter(v=>v!=null);
const lastv=a=>{for(let i=a.length-1;i>=0;i--)if(a[i]!=null)return a[i];return null};
const dayOf=t=>new Date((t+TZ)*1e3).toISOString().slice(0,10);
const ago=t=>{const s=NOW()-t;return s<90?'1 min':s<5400?Math.round(s/60)+' min':s<172800?Math.round(s/3600)+' h':Math.round(s/86400)+' dias'};
const nodeName=id=>id==null?'—':'N'+String(id).padStart(2,'0');
// Linha de dados: [ts, ...COLS, seq, flags] (ver API.readings)
const COLS=['ti','ri','to','ro','rms','pk','b0','b1','b2','b3','vb'],NC=COLS.length,SEQ=NC+1,FL=NC+2;
const MODE={pk:'max',rms:'q'};
const V={rms:{name:'Vibração RMS',u:'mg',color:'#b45309',dp:1},pk:{name:'Vibração de pico',u:'mg',color:'#7c3aed',dp:1},
 b0:{name:'50–100 Hz',u:'dB',color:'#b45309',dp:1},b1:{name:'100–200 Hz',u:'dB',color:'#0f766e',dp:1},b2:{name:'200–350 Hz',u:'dB',color:'#7c3aed',dp:1},b3:{name:'350–600 Hz',u:'dB',color:'#be185d',dp:1},
 vb:{name:'Tensão da bateria',u:'V',color:'#15803d',dp:3},ti:{name:'Temperatura interna',u:'°C',color:IN,dp:1,ideal:C.IDEAL_T_IN},to:{name:'Temperatura externa',u:'°C',color:OUT,dp:1},
 ri:{name:'Umidade interna',u:'%',color:IN,dash:1,r:1,dp:1,ideal:C.IDEAL_RH_IN},ro:{name:'Umidade externa',u:'%',color:OUT,dash:1,r:1,dp:1}};
const POWER=['normal','economia','crítico','hibernação'];
// Bits de flags (arquitetura v1.2, seção 5.3) que indicam problema no nó
const FLAGS=[[0,'SHT30 interno','sem leitura válida'],[1,'SHT30 externo','sem leitura válida'],[11,'SHT30 interno encoberto','leitura parada'],
 [2,'ADXL345','DEVID inválido ou erro de SPI'],[3,'FIFO do acelerômetro','overrun na janela'],[8,'Janela de vibração','não executada'],
 [7,'Cartão SD','escrita falhou'],[4,'Relógio (DS1302)','sem hora válida']];
const bit=(fl,b)=>fl!=null&&(fl>>b)&1;

const S={nid:null,gid:null,status:null,gw:null,rows:new Map(),from:null,loading:true,busy:false,err:null,okAt:null,maxSeq:undefined,lc:undefined,none:false};
let D={cols:COLS,rows:[],days:[],start:null,end:null};
const TD=dayOf(NOW());
let page=location.hash=='#sistema'?'sys':'tel',rg='d',day=TD,W=[0,0],B0=30,cu={g:'dia',d0:dayOf(NOW()-6*86400),d1:TD,m0:TD.slice(0,7),m1:TD.slice(0,7),hf:'off',h0:'06:00',h1:'18:00'};const CHD={},HID={},CHA={};
const dS=d=>Date.parse(d+'T00:00:00Z')/1e3-TZ,mm=s=>{const m=(s||'0:0').split(':');return +m[0]*60+ +m[1]};
const lastDay=()=>D.days.length?D.days[D.days.length-1][0]:dayOf(NOW());

function build(){const rows=[...S.rows.values()].sort((a,b)=>a[0]-b[0]),dm=new Map();
 rows.forEach(r=>{const d=dayOf(r[0]);dm.set(d,(dm.get(d)||0)+1)});
 D={cols:COLS,rows,days:[...dm],start:rows.length?rows[0][0]:null,end:rows.length?rows[rows.length-1][0]:null}}
function merge(rows){rows.forEach(r=>S.rows.set(r[SEQ]!=null?r[SEQ]:'t'+r[0],r))}

function win(){let w;
 if(rg=='x')w=[dS(day),dS(day)+86400];
 else if(rg=='c'){const ld=lastDay();let a,b;
  if(cu.hf=='ult'){a=ld;b=ld}else if(cu.g=='mes'){const[y1,m1]=cu.m1.split('-').map(Number);a=cu.m0+'-01';b=new Date(Date.UTC(y1,m1,0)).toISOString().slice(0,10)}else{a=cu.d0;b=cu.d1}
  if(a>b){const z=a;a=b;b=z}w=[dS(a),dS(b)+86400]}
 else w=[NOW()-{d:1,w:7,m:30}[rg]*86400,NOW()];
 const n=(w[1]-w[0])/86400;B0=rg=='d'||rg=='x'?30:n<=1.5?30:n<=10?600:3600;return w}
const ok=t=>{if(t<W[0]||t>W[1])return false;if(rg!='c'||cu.hf=='off')return true;const m=Math.floor((((t+TZ)%86400)+86400)%86400/60),a=mm(cu.h0),b=mm(cu.h1);return a<=b?m>=a&&m<=b:m>=a||m<=b};
// Agrega em baldes de B0 s; valores ausentes (sentinela) não entram na conta e um balde sem valor vira lacuna.
function agg(rows,cols){const B=B0,M=new Map(),n=cols.length;
 for(const r of rows){if(!ok(r[0]))continue;const k=Math.floor(r[0]/B)*B;let o=M.get(k);if(!o){o={c:Array(n).fill(0),s:Array(n).fill(0)};M.set(k,o)}
  for(let j=0;j<n;j++){const v=r[1+j];if(v==null)continue;const m=MODE[cols[j]];o.s[j]=m=='max'?(o.c[j]?Math.max(o.s[j],v):v):m=='q'?o.s[j]+v*v:o.s[j]+v;o.c[j]++}}
 const A={t:[],v:{},gap:Math.max(3*B,C.GAP_S)};cols.forEach(c=>A.v[c]=[]);
 [...M.keys()].sort((x,y)=>x-y).forEach(k=>{const o=M.get(k);A.t.push(k+B/2);cols.forEach((c,j)=>{const q=o.c[j];A.v[c].push(!q?null:MODE[c]=='max'?o.s[j]:MODE[c]=='q'?Math.sqrt(o.s[j]/q):o.s[j]/q)})});return A}

function chart(id,title,ic,src,keys,ideal,dual,note){
 const Wd=560,Hh=260,pl=52,pt=18,pb=28,N=src.t.length;CHA[id]=[id,title,ic,src,keys,ideal,dual,note];
 const ss=N?keys.map(k=>({...V[k],key:k,hid:HID[id+k],ax:dual&&V[k].r?1:0,v:src.v[k]})).filter(s=>s.v.some(x=>x!=null)):[];
 if(!ss.length){delete CHD[id];return `<section class="card" id="cc-${id}"><h3>${ico(ic)}${title}</h3><div class="hv">Sem dados neste período.</div></section>`}
 let vis=ss.filter(s=>!s.hid);if(!vis.length){ss.forEach(s=>{s.hid=HID[id+s.key]=false});vis=ss}if(vis.length==1)vis[0].ax=0;const pr=vis.some(s=>s.ax==1)?52:12;
 const x0=src.t[0],x1=Math.max(src.t[N-1],x0+1),X=t=>pl+(t-x0)/(x1-x0)*(Wd-pl-pr);
 const ax=[0,1].map(a=>{const Vv=ss.filter(s=>s.ax==a&&!s.hid);if(!Vv.length)return null;const v=nn(Vv.flatMap(s=>s.v));
  let lo=Math.min(...v),hi=Math.max(...v);const sp0=hi-lo||1;Vv.forEach(s=>{if(s.ideal&&s.ideal[0]<=hi+sp0*1.5+.5&&s.ideal[1]>=lo-sp0*1.5-.5){lo=Math.min(lo,s.ideal[0]);hi=Math.max(hi,s.ideal[1])}});const p=(hi-lo)*.1||1;return[lo-p,hi+p]});
 const Y=(v,a)=>Hh-pb-(v-ax[a][0])/(ax[a][1]-ax[a][0])*(Hh-pb-pt);
 const tx=(x,y,t,a='start')=>`<text x="${x}" y="${y}" font-size="12" fill="#6b7280" text-anchor="${a}">${t}</text>`;
 let g='';vis.filter(s=>s.ideal).forEach((s,j)=>{const T0=Hh-pb,T1=pt,y1=Y(s.ideal[1],s.ax),y2=Y(s.ideal[0],s.ax),t=`Faixa ideal ${s.ideal[0]}–${s.ideal[1]} ${s.u}`,top=Math.max(T1,y1),bot=Math.min(T0,y2);
  if(bot<=top){const up=y2<=T1;g+=tx(j?Wd-pr-6:pl+6,up?T1+14:T0-6,(up?'▲ ':'▼ ')+t+(up?' (acima do gráfico)':' (abaixo do gráfico)'),j?'end':'start');return}
  g+=j?`<rect x="${pl}" y="${top}" width="${Wd-pl-pr}" height="${bot-top}" fill="none" stroke="${s.color}" stroke-dasharray="4 3"/>`+tx(Wd-pr-6,top+14,t,'end'):`<rect x="${pl}" y="${top}" width="${Wd-pl-pr}" height="${bot-top}" fill="#dcfce7"/>`+tx(pl+6,top+14,t)});
 for(let j=0;j<=4;j++){const y=Hh-pb-j/4*(Hh-pb-pt);g+=`<line x1="${pl}" x2="${Wd-pr}" y1="${y}" y2="${y}" stroke="#ece9e0"/>`;
  [0,1].forEach(a=>{if(ax[a]){const v=ax[a][0]+j/4*(ax[a][1]-ax[a][0]);g+=tx(a?Wd-pr+4:pl-4,y+4,v.toFixed((ax[a][1]-ax[a][0])/4>=1?0:(ax[a][1]-ax[a][0])/4>=.1?1:(ax[a][1]-ax[a][0])/4>=.01?2:3),a?'start':'end')}})}
 g+=tx(pl-4,10,vis.find(s=>s.ax==0).u,'end')+(vis.find(s=>s.ax==1)?tx(Wd-pr+4,10,vis.find(s=>s.ax==1).u):'');
 const sp=x1-x0,lab=t=>fmt(t,sp<=129600?{hour:'2-digit',minute:'2-digit'}:sp<=864000?FF:{day:'2-digit',month:'2-digit'});
 for(let j=0;j<5;j++)g+=tx(pl+j*(Wd-pl-pr)/4,Hh-8,lab(x0+j*sp/4),j?j==4?'end':'middle':'start');
 vis.forEach(s=>{let cur=[],lt=null;const fl=()=>{if(cur.length==1)g+=`<circle cx="${cur[0][0]}" cy="${cur[0][1]}" r="3" fill="${s.color}"/>`;
   else if(cur.length>1)g+=`<polyline fill="none" stroke="${s.color}" stroke-width="2.5" ${s.dash?'stroke-dasharray="5 3"':''} points="${cur.map(c=>c.join(',')).join(' ')}"/>`;cur=[]};
  src.t.forEach((t,i)=>{const v=s.v[i];if(v==null){fl();return}if(lt!=null&&t-lt>src.gap)fl();cur.push([+X(t).toFixed(1),+Y(v,s.ax).toFixed(1)]);lt=t});fl()});
 CHD[id]={ss,src,pl,pr,Wd,x0,x1};
 const st=s=>{const v=nn(s.v);return `mín ${Math.min(...v).toFixed(s.dp)} · méd ${(v.reduce((a,b)=>a+b)/v.length).toFixed(s.dp)} · máx ${Math.max(...v).toFixed(s.dp)} ${s.u}`};
 return `<section class="card" id="cc-${id}"><h3>${ico(ic)}${title}</h3><div class="hv" id="hv-${id}">${note||'Toque ou passe o mouse sobre o gráfico para ver os valores.'}</div>
 <svg class="ch" data-c="${id}" viewBox="0 0 ${Wd} ${Hh}">${g}<line class="cur" y1="${pt}" y2="${Hh-pb}"/></svg>
 <div class="lg">${ss.map(s=>`<button class="lgi ${s.hid?'off':(ss.length>1&&vis.length==1?'on1':'')}" data-c="${id}" data-k="${s.key}"><i style="border-top-style:${s.dash?'dashed':'solid'};border-color:${s.color}"></i><b>${s.name}</b> agora ${f(lastv(s.v),s.dp)} ${s.u}<small>${st(s)}</small></button>`).join('')}</div>${ss.length>1?'<div class="hv" style="margin:6px 0 0">Clique em uma medida para ver só ela; clique de novo para mostrar todas.</div>':''}</section>`}
const m=(ic,l,v,u,c)=>`<div class="m">${ico(ic)}<div><small>${l}</small><b>${v}</b><span>${u}</span>${c||''}</div></div>`;
const chip=(v,r)=>v==null?'':v>r[1]?'<em class="tg wa">Acima do ideal</em>':v<r[0]?'<em class="tg wa">Abaixo do ideal</em>':'<em class="tg ok2">Na faixa ideal</em>';
const sgn=v=>v==null?'—':(v>0?'+':'')+v.toFixed(1);
const tile=(ic,h,v,s)=>`<div class="card"><h3>${ico(ic)}${h}</h3><div class="m"><div><b>${v}</b></div></div><div class="hv" style="margin-top:4px">${s}</div></div>`;

function empty(){
 if(S.loading)return `<div class="card"><h3>${ico('clock')}Carregando dados da API…</h3></div>`;
 if(!D.rows.length&&S.err&&!S.okAt)return `<div class="card"><h3>${ico('srv')}Não foi possível carregar os dados</h3><p class="hv">Confira a URL em js/config.js e se a API libera CORS para este endereço. Nova tentativa a cada ${C.REFRESH_S} s.</p></div>`;
 if(!D.rows.length)return `<div class="card"><h3>${ico('hex')}Ainda não há registros</h3><p class="hv">${S.none?'A API não listou nenhum nó em GET /v1/nodes.':'O nó '+nodeName(S.nid)+' ainda não tem leituras nos últimos '+C.INITIAL_DAYS+' dias.'}</p></div>`;
 return `<div class="card"><h3>${ico('hex')}Sem dados neste período</h3><p class="hv">Há dados de ${fmt(D.start,FF)} a ${fmt(D.end,FF)}, em ${D.days.length} ${D.days.length==1?'dia':'dias'} de coleta.</p><button class="btn" onclick="goLast()">Ver o último dia com dados</button></div>`}
function goLast(){rg='x';day=lastDay();render()}

function pTel(A){const N=A.t.length;if(!N)return empty();
 const L=k=>lastv(A.v[k]),ti=L('ti'),ri=L('ri'),to=L('to'),ro=L('ro'),dt=ti!=null&&to!=null?ti-to:null,dr=ri!=null&&ro!=null?ri-ro:null;
 const T=C.IDEAL_T_IN,H=C.IDEAL_RH_IN;
 return `<h2>Valores mais recentes do período <small>(${fmt(A.t[N-1],FF)})</small></h2><div class="sum">
 <div class="card top-in"><h3>${ico('hex')}Interno</h3><div class="ms">${m('thermo','Temperatura',f(ti,1),'°C',chip(ti,T))}${m('drop','Umidade',f(ri,0),'%',chip(ri,H))}</div></div>
 <div class="card top-out"><h3>${ico('sig')}Externo</h3><div class="ms">${m('thermo','Temperatura',f(to,1),'°C')}${m('drop','Umidade',f(ro,0),'%')}</div></div>
 <div class="card top-n"><h3>${ico('cmp')}Diferença (interno − externo)</h3><div class="ms">${m('thermo','Temperatura',sgn(dt),'°C')}${m('drop','Umidade',sgn(dr),'p.p.')}</div>
 ${dt!=null&&dr!=null?`<div class="hv" style="margin-top:8px">Interno ${Math.abs(dt).toFixed(1)} °C ${dt>=0?'mais quente':'mais frio'} e ${Math.abs(dr).toFixed(0)} p.p. ${dr>=0?'mais úmido':'mais seco'} que o externo.</div>`:''}</div></div>
 <h2>Interno e externo no mesmo card</h2><div class="grid">${chart('c1','Interno: temperatura e umidade','hex',A,['ti','ri'],T,1,'Faixa verde: intervalo ideal da temperatura interna. Faixa tracejada: ideal da umidade (eixo direito).')}${chart('c2','Externo: temperatura e umidade','sig',A,['to','ro'],null,1,'Tracejada: umidade.')}</div>
 <h2>Comparação interno × externo</h2><div class="grid">${chart('c3','Temperatura: interna × externa','thermo',A,['ti','to'],T,0,`Faixa verde: intervalo ideal da temperatura interna (${T[0]} a ${T[1]} °C, a validar).`)}${chart('c4','Umidade: interna × externa','drop',A,['ri','ro'],H,0,`Faixa verde: intervalo ideal da umidade interna (${H[0]} a ${H[1]} %, a validar).`)}</div>
 <h2>Vibração <small>(acelerômetro ADXL345)</small></h2><div class="grid">${chart('c5','Vibração: RMS e pico','wave',A,['rms','pk'],null,0,'Medida pelo acelerômetro em janelas de 10 s (RMS e pico, em mg).')}${chart('c6','Energia por faixa de frequência','wave',A,['b0','b1','b2','b3'],null,0,'Nível em dB em quatro faixas de frequência. Clique em uma faixa para ver só ela.')}</div>`}

function pSys(){if(!D.rows.length&&!S.status&&!S.gw)return empty();
 const A=agg(D.rows,D.cols),st=S.status||{},g=S.gw,nm=nodeName(S.nid),inW=D.rows.filter(r=>ok(r[0])),lr=D.rows[D.rows.length-1],lf=lr?lr[FL]:null;
 const lc=st.last_contact!=null?st.last_contact:D.end,pw=st.power_level!=null?st.power_level:lf!=null?(lf>>9)&3:null;
 const h=st.attempts,sum=h?h.reduce((a,b)=>a+b,0):0,vb=lastv(A.v.vb);
 const ok2=t=>`<em class="tg ok2">${t}</em>`,wa=t=>`<em class="tg wa">${t}</em>`;
 const health=FLAGS.map(([b,n,why])=>{const c=inW.filter(r=>bit(r[FL],b)).length;
  return `<tr><td>${n}</td><td>${lf==null?'—':bit(lf,b)?wa(why):ok2('ok')}</td><td>${inW.length?c+' de '+inW.length:'—'}</td></tr>`}).join('');
 return `<h2>Enlace LoRa <small>(nó ${nm})</small></h2><div class="sum">
 ${tile('clock','Último contato',lc==null?'—':ago(lc),lc==null?'Sem contato registrado':fmt(lc,FF)+' · alerta após '+C.NODE_SILENT_MIN+' min')}
 ${tile('sig','Sinal do último ACK',st.rssi==null?'—':st.rssi+' dBm',st.snr==null?(st.rssi==null?'RSSI/SNR não informados (RSSI por pacote desligado no módulo?)':'SNR não informado'):'SNR '+st.snr+' dB')}
 ${tile('que','Pendentes no nó',st.backlog==null?'—':st.backlog,'Registros ainda não confirmados (backlog)')}
 ${tile('chk','Entrega na 1ª tentativa',sum?Math.round(h[0]/sum*100)+' %':'—',sum?sum+' quadros contabilizados':'Histograma de tentativas não informado')}
 ${tile('srv','Registros',st.max_seq!=null?st.max_seq:'—',`Último seq do nó · ${inW.length} no período`)}
 ${tile('cmp','Sessões falhas seguidas',st.failed_sessions==null?'—':st.failed_sessions,'Sessões LoRa sem ACK desde o último sucesso')}</div>
 <h2>Energia</h2><div class="sum">
 ${tile('batt','Bateria',vb==null?'—':vb.toFixed(2)+' V',st.vbat_load!=null?'Sob carga (STATUS): '+st.vbat_load.toFixed(2)+' V':'Último registro do período')}
 ${tile('wave','Nível de energia',pw==null?'—':POWER[pw],pw?wa('Nó economizando energia'):pw===0?ok2('Operação normal'):'')}
 ${tile('chk','Carregamento',lf==null?'—':bit(lf,13)?'Carregando':'Não carregando','Pino CHRG do CN3065, último registro')}</div>
 <div class="grid">${chart('s1','Tensão da bateria','batt',A,['vb'],null,0,'Leitura do ADC no ciclo do nó.')}
 ${h?`<section class="card"><h3>${ico('chk')}Tentativas até o ACK</h3><div class="hv">Quantos quadros precisaram de 1 a ${h.length} tentativas.</div><div style="display:grid;grid-template-columns:auto 1fr;gap:6px 10px;font-size:14px;align-items:center">${h.map((n,i)=>`<span>${i+1}ª tentativa</span><div><div style="height:10px;background:var(--bd);border-radius:5px;overflow:hidden"><i style="display:block;height:100%;width:${sum?n/sum*100:0}%;background:var(--ac)"></i></div><small>${n} quadros · ${sum?(n/sum*100).toFixed(0):0} %</small></div>`).join('')}</div></section>`:''}</div>
 <h2>Saúde do nó</h2><div class="card tw"><table><tr><th>Componente</th><th>Último registro</th><th>Falhas no período</th></tr>${health}
 <tr><td>Espaço livre no SD</td><td colspan="2">${st.sd_free_mb==null?'—':st.sd_free_mb+' MB'}${st.sd_errors?' · '+st.sd_errors+' erros':''}</td></tr>
 <tr><td>Motivo do último reset</td><td colspan="2">${st.reset_reason==null?'—':esc(st.reset_reason)}${bit(lf,6)?' · último registro é o primeiro após reset':''}</td></tr>
 <tr><td>Firmware</td><td colspan="2">${st.fw_version==null?'—':esc(st.fw_version)}</td></tr></table></div>
 <h2>Central <small>(${esc(S.gid||'—')})</small></h2><div class="sum">${!g?tile('srv','Último heartbeat','—','A API não informou o estado da central'):
 tile('clock','Último heartbeat',g.time==null?'—':ago(g.time),(g.time==null?'':fmt(g.time,FF)+' · ')+'alerta após '+C.GW_SILENT_MIN+' min')+
 tile('que','Fila de envio',f(g.queue,0),`${g.quarantine?wa(g.quarantine+' em quarentena'):'Nenhum item em quarentena'}`)+
 tile('sig','Wi-Fi',g.wifi_rssi==null?'—':g.wifi_rssi+' dBm','RSSI da rede da casa')+
 tile('clock','Relógio (NTP)',g.ntp_ok==null?'—':g.ntp_ok?'Sincronizado':'Sem NTP',g.ntp_ok===false?wa('Hora da central não confiável'):'')+
 tile('srv','Em funcionamento',g.uptime_s==null?'—':ago(NOW()-g.uptime_s),`Memória livre: ${g.free_heap==null?'—':Math.round(g.free_heap/1024)+' KB'} · firmware ${g.fw_version==null?'—':esc(g.fw_version)}`)}</div>`}

// Estados do nó (doc. de nuvem, seção 8.2): primeiro a central, depois o nó.
function alerts(){const a=[],n=NOW(),st=S.status||{};
 if(S.err)a.push(['er',`Sem conexão com a API (${esc(S.err)}).${S.okAt?' Mostrando dados de '+fmt(S.okAt,FF)+', sem atualização.':''}`]);
 if(S.gw&&S.gw.time!=null&&n-S.gw.time>C.GW_SILENT_MIN*60)a.push(['wa',`Central ${esc(S.gid)} sem contato: último heartbeat há ${ago(S.gw.time)}. Estado do nó desconhecido.`]);
 else if(S.nid!=null){const lc=st.last_contact!=null?st.last_contact:D.end;
  if(lc!=null&&n-lc>C.NODE_SILENT_MIN*60)a.push(['wa',`Nó ${nodeName(S.nid)} silencioso: último contato há ${ago(lc)}.`]);
  if(st.power_level>=1)a.push(['wa',`Nó ${nodeName(S.nid)} em modo de energia ${POWER[st.power_level]}.`])}
 return a.map(([c,t])=>`<div class="al ${c}">${t}</div>`).join('')}

function render(){W=win();const A=agg(D.rows,D.cols),fm=t=>fmt(t,FF);
 if(S.nid!=null&&S.from!=null&&W[0]<S.from&&!S.busy)extend(W[0]);
 $('last').textContent=D.end!=null?'Último dado: '+fm(D.end):S.loading?'Carregando…':'Sem dados';
 $('upd').textContent=(API.isMock?'Demonstração · ':'')+(S.okAt?'atualizado às '+fmt(S.okAt,HMS):'');
 $('alert').innerHTML=alerts();
 $('foot').textContent=(API.isMock?'Modo de demonstração: leituras do teste LoRa de 05/10/2026 deslocadas para agora; status e heartbeat são ilustrativos. ':'Dados da API BeeCooler (AWS). ')+
  `Nó ${nodeName(S.nid)}, consulta a cada ${C.REFRESH_S} s. Horários de Brasília. Faixas ideais a validar com apicultor.`;
 const hd=rg=='c'&&cu.hf!='off'?`, das ${cu.h0} às ${cu.h1}${cu.hf=='ult'?' no último dia registrado':' em cada dia do período'}`:'';
 $('per').textContent='Período: '+fm(W[0])+' até '+fm(W[1]-1)+hd+'.'+(S.busy?' Buscando dados do período na API…':'')+' Lacunas sem dados aparecem como quebras na linha.';
 $('dt').style.display=rg=='x'?'':'none';syncCp();$('dt').value=day;$('dt').max=TD;
 $('days').innerHTML=rg=='x'&&D.days.length?'Dias com dados: '+D.days.slice(-14).map(d=>`<button data-d="${d[0]}" class="${d[0]==day?'on':''}">${d[0].slice(8)}/${d[0].slice(5,7)}</button>`).join(''):'';
 document.querySelectorAll('#tabs button').forEach(b=>b.classList.toggle('on',b.dataset.p==page));
 document.querySelectorAll('#rg button').forEach(b=>b.classList.toggle('on',b.dataset.r==rg));
 $('panel').innerHTML=page=='tel'?pTel(A):pSys()}
function syncCp(){const on=rg=='c';$('cp').style.display=on?'':'none';if(!on)return;const dm=cu.g=='mes';
 ['cd0','cd1'].forEach(i=>$(i).style.display=dm?'none':'');['cm0','cm1'].forEach(i=>$(i).style.display=dm?'':'none');
 $('cd0').value=cu.d0;$('cd1').value=cu.d1;$('cm0').value=cu.m0;$('cm1').value=cu.m1;$('chf').value=cu.hf;$('ch0').value=cu.h0;$('ch1').value=cu.h1;
 $('chh').style.display=cu.hf=='off'?'none':'';['cd0','cd1'].forEach(i=>$(i).max=TD);['cm0','cm1'].forEach(i=>$(i).max=TD.slice(0,7));
 document.querySelectorAll('#cg button').forEach(b=>b.classList.toggle('on',b.dataset.g==cu.g));
 ['cd0','cd1','cm0','cm1'].forEach(i=>$(i).disabled=cu.hf=='ult')}

// A cada consulta: GET /v1/nodes; leituras e STATUS só quando max_seq ou o último contato mudam (D61).
async function poll(first){
 try{const ns=await API.nodes(),nd=(C.NODE_ID!=null&&(ns.find(x=>x.node_id==C.NODE_ID)||{node_id:C.NODE_ID}))||ns[0];
  S.none=!nd;
  if(nd){S.nid=nd.node_id;S.gid=nd.gateway_id||C.GATEWAY_ID;
   if(first||nd.max_seq!==S.maxSeq||nd.last_contact!==S.lc){const to=NOW()+300;
    if(first||D.end==null){S.from=NOW()-C.INITIAL_DAYS*86400;merge(await API.readings(S.nid,S.from,to))}
    else{merge(await API.readings(S.nid,D.end-7200,to));
     // Backlog drenado depois de uma queda chega com ts antigo: se ainda falta o max_seq, relê o histórico carregado.
     if(nd.max_seq!=null&&!S.rows.has(nd.max_seq))merge(await API.readings(S.nid,S.from,to))}
    S.status=await API.status(S.nid);S.maxSeq=nd.max_seq;S.lc=nd.last_contact}}
  if(S.gid)S.gw=await API.gateway(S.gid);
  S.err=null;S.okAt=NOW()}
 catch(e){S.err=e.message||String(e);console.error(e)}
 S.loading=false;build();render()}
async function extend(a){S.busy=true;
 // Mesmo com erro, não tenta de novo o mesmo trecho a cada render; o alerta de erro fica na tela.
 try{merge(await API.readings(S.nid,a,S.from))}catch(e){S.err=e.message||String(e);console.error(e)}S.from=a;
 S.busy=false;build();render()}

$('cp').addEventListener('change',e=>{const i=e.target.id,v=e.target.value;if(i=='chf')cu.hf=v;else if(v){if(i=='cd0')cu.d0=v;if(i=='cd1')cu.d1=v;if(i=='cm0')cu.m0=v;if(i=='cm1')cu.m1=v;if(i=='ch0')cu.h0=v;if(i=='ch1')cu.h1=v}render()});
$('cg').onclick=e=>{const b=e.target.closest('button');if(b){cu.g=b.dataset.g;render()}};
$('tabs').onclick=e=>{const b=e.target.closest('button');if(b){page=b.dataset.p;history.replaceState(null,'',page=='sys'?'#sistema':location.pathname+location.search);render()}};
$('rg').onclick=e=>{const b=e.target.closest('button');if(b){rg=b.dataset.r;if(rg=='x')day=lastDay();render()}};
$('dt').onchange=e=>{if(e.target.value){day=e.target.value;render()}};
$('days').onclick=e=>{const b=e.target.closest('button');if(b){day=b.dataset.d;render()}};
function hover(e){const sv=e.target.closest&&e.target.closest('svg[data-c]');if(!sv||!CHD[sv.dataset.c])return;
 const id=sv.dataset.c,{ss,src,pl,pr,Wd,x0,x1}=CHD[id],r=sv.getBoundingClientRect(),x=(e.clientX-r.left)/r.width*Wd,tt=x0+(x-pl)/(Wd-pl-pr)*(x1-x0);
 let i=0,bd=1e12;src.t.forEach((t,j)=>{const d=Math.abs(t-tt);if(d<bd){bd=d;i=j}});
 const px=pl+(src.t[i]-x0)/(x1-x0)*(Wd-pl-pr),l=sv.querySelector('.cur');l.setAttribute('x1',px);l.setAttribute('x2',px);l.style.opacity=1;
 $('hv-'+id).innerHTML='<b>'+fmt(src.t[i],FF)+'</b> · '+ss.filter(s=>!s.hid).map(s=>s.name.toLowerCase()+' '+(s.v[i]==null?'—':s.v[i].toFixed(s.dp)+' '+s.u)).join(' · ')}
document.addEventListener('pointermove',hover);document.addEventListener('pointerdown',hover);
document.addEventListener('pointerout',e=>{const sv=e.target.closest&&e.target.closest('svg[data-c]');if(sv)sv.querySelector('.cur').style.opacity=0});
document.addEventListener('click',e=>{const b=e.target.closest('.lgi');if(!b)return;const id=b.dataset.c,k=b.dataset.k;
 const ks=CHD[id]?CHD[id].ss.map(s=>s.key):[];if(ks.length<2)return;const only=ks.every(x=>x==k?!HID[id+x]:HID[id+x]);ks.forEach(x=>HID[id+x]=only?false:x!=k);$('cc-'+id).outerHTML=chart(...CHA[id])});

(async()=>{
 if(new URLSearchParams(location.search).has('mock'))await API.useMock();
 else if(!C.API_BASE){S.loading=false;S.err='API_BASE não configurada em js/config.js';render();return}
 render();await poll(true);
 setInterval(()=>{if(!document.hidden)poll(false)},C.REFRESH_S*1e3);
 document.addEventListener('visibilitychange',()=>{if(!document.hidden&&S.okAt&&NOW()-S.okAt>C.REFRESH_S)poll(false)});
})();
