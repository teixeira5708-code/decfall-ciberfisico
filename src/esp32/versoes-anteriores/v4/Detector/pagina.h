#pragma once
#include <Arduino.h>

const char PAGINA_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Decfall</title>
<style>
:root{--fundo:#e9eff2;--papel:#fbfdfd;--tinta:#12232e;--suave:#526673;--linha:#c5d4db;--ok:#0b7a75;--alarme:#d7263d;--aviso:#e8a317}
*{box-sizing:border-box}
[hidden]{display:none!important}
body{margin:0;background:var(--fundo);color:var(--tinta);font-family:"Avenir Next","Segoe UI",system-ui,sans-serif;line-height:1.45}
.pagina{max-width:980px;margin:0 auto;padding:0 16px 40px}
header{display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:10px;padding:18px 0 10px}
h1{margin:0;font-size:1.5rem;font-weight:800;letter-spacing:-.02em}
h1 small{display:block;font-size:.8rem;font-weight:500;color:var(--suave);letter-spacing:0}
.estado{display:flex;gap:8px;flex-wrap:wrap}
.selo{display:inline-flex;align-items:center;gap:7px;padding:6px 12px;border-radius:999px;background:#fff;border:1px solid var(--linha);font-size:.85rem;font-weight:600}
.selo i{width:9px;height:9px;border-radius:50%;background:var(--suave)}
.selo.ok i{background:var(--ok)}
.selo.aviso i{background:var(--aviso)}
.selo.alarme{background:var(--alarme);border-color:var(--alarme);color:#fff}
.selo.alarme i{background:#fff}
.abas{display:flex;gap:22px;border-bottom:1px solid var(--linha);margin:8px 0 18px}
.aba-btn{background:none;border:0;padding:10px 2px;margin-bottom:-1px;font:inherit;font-weight:600;color:var(--suave);cursor:pointer;border-bottom:3px solid transparent}
.aba-btn.ativa{color:var(--tinta);border-bottom-color:var(--ok)}
.aba{display:none}
.aba.ativa{display:block}
.leitura{display:flex;flex-wrap:wrap;align-items:flex-end;gap:10px 40px;margin-bottom:14px}
.grande{font-size:3.4rem;font-weight:800;line-height:1;letter-spacing:-.03em;font-variant-numeric:tabular-nums}
.grande em{font-style:normal}
.grande span{font-size:1.1rem;font-weight:600;color:var(--suave);margin-left:6px;letter-spacing:0}
.eixos{display:flex;gap:26px;font-variant-numeric:tabular-nums}
.eixos div{min-width:64px}
.eixos b{display:block;font-size:1.25rem}
.eixos small{color:var(--suave)}
.painel{background:var(--papel);border:1px solid var(--linha);border-radius:6px;padding:12px}
canvas{display:block;width:100%;height:280px}
.legenda{display:flex;gap:18px;flex-wrap:wrap;margin-top:8px;font-size:.82rem;color:var(--suave)}
.legenda span::before{content:"";display:inline-block;width:18px;margin-right:6px;vertical-align:middle;border-top:2px solid}
.legenda .tr::before{border-color:var(--tinta)}
.legenda .lq::before{border-color:var(--aviso);border-top-style:dashed}
.legenda .li::before{border-color:var(--alarme);border-top-style:dashed}
.acoes{display:flex;gap:10px;flex-wrap:wrap;margin-top:14px}
.botao{font:inherit;font-weight:600;padding:10px 16px;border-radius:6px;border:1px solid var(--tinta);background:transparent;color:var(--tinta);cursor:pointer}
.botao:hover{background:var(--tinta);color:#fff}
button:focus-visible{outline:3px solid var(--aviso);outline-offset:2px}
.resumo{display:flex;gap:28px;flex-wrap:wrap;margin-bottom:12px;color:var(--suave)}
.resumo b{margin-right:6px;font-size:1.5rem;color:var(--tinta)}
.rolagem{overflow-x:auto}
table{width:100%;border-collapse:collapse;background:var(--papel);border:1px solid var(--linha)}
th,td{text-align:left;padding:10px 12px;border-bottom:1px solid var(--linha);font-size:.92rem;white-space:nowrap}
th{color:var(--suave);font-weight:600;font-size:.82rem}
.marca{display:inline-block;padding:3px 10px;border-radius:4px;font-weight:700;font-size:.82rem}
.m-pendente{background:#fff0c4;color:#6b4b00}
.m-erro{background:#d9ece9;color:#0b5f5a}
.m-verdadeira{background:var(--alarme);color:#fff}
.vazio{padding:28px;text-align:center;color:var(--suave)}
.alarme{position:fixed;inset:0;z-index:10;display:flex;align-items:center;justify-content:center;padding:20px;text-align:center;color:#fff;background:var(--alarme);animation:pulsar 1.2s ease-in-out infinite}
.alarme.confirmada{background:#8c1020}
.alarme h2{margin:0 0 12px;font-size:clamp(2.2rem,8vw,4.5rem);line-height:1.02;letter-spacing:-.02em}
.alarme p{margin:0 auto 28px;max-width:32ch;font-size:1.2rem}
.alarme button{width:min(100%,460px);padding:18px 24px;border:0;border-radius:8px;background:#fff;color:var(--alarme);font:inherit;font-size:1.25rem;font-weight:800;cursor:pointer}
@keyframes pulsar{50%{filter:brightness(.8)}}
@media (prefers-reduced-motion:reduce){.alarme{animation:none}}
</style>
</head>
<body>
<div class="pagina">
<header>
<h1>Decfall<small>Detector de quedas</small></h1>
<div class="estado">
<span class="selo" id="seloEstado"><i></i><b>Conectando</b></span>
<span class="selo" id="seloWifi"><i></i><span>Wi-Fi</span></span>
<span class="selo" id="seloSensor"><i></i><span>Acelerômetro</span></span>
</div>
</header>
<nav class="abas">
<button class="aba-btn ativa" data-aba="monitor">Monitor</button>
<button class="aba-btn" data-aba="historico">Histórico de quedas <span id="contador"></span></button>
</nav>
<main>
<section id="monitor" class="aba ativa">
<div class="leitura">
<div class="grande"><em id="vTotal">--</em><span>m/s²</span></div>
<div class="eixos">
<div><b id="vX">--</b><small>eixo x</small></div>
<div><b id="vY">--</b><small>eixo y</small></div>
<div><b id="vZ">--</b><small>eixo z</small></div>
</div>
</div>
<div class="painel">
<canvas id="grafico"></canvas>
<div class="legenda">
<span class="tr">Aceleração total, últimos 10 s</span>
<span class="lq">Queda livre abaixo de <b id="tQuedaLivre">2</b></span>
<span class="li">Impacto acima de <b id="tImpacto">10</b></span>
</div>
</div>
<div class="acoes">
<button class="botao" id="bBuzzer">Testar buzzer</button>
<button class="botao" id="bSimular">Simular queda</button>
</div>
</section>
<section id="historico" class="aba">
<div class="resumo">
<div><b id="rTotal">0</b>quedas registradas</div>
<div><b id="rVerdadeiras">0</b>verdadeiras</div>
<div><b id="rErros">0</b>anuladas como erro</div>
</div>
<div class="rolagem">
<table>
<thead><tr><th>Nº</th><th>Horário</th><th>Pico (m/s²)</th><th>Status</th><th>Desativada por</th></tr></thead>
<tbody id="corpo"></tbody>
</table>
</div>
<div class="vazio" id="vazio">Nenhuma queda registrada ainda.</div>
</section>
</main>
</div>
<div class="alarme" id="alarme" hidden>
<div>
<h2 id="alarmeTitulo">Possível queda detectada</h2>
<p id="alarmeTexto"></p>
<button id="bCancelar">É erro: desativar alarme</button>
</div>
</div>
<script>
const $ = id => document.getElementById(id);
const MAX_PONTOS = 500;
const PONTOS_POR_SEGUNDO = 50;
const NOMES_STATUS = { pendente: 'Pendente', erro: 'Erro (anulada)', verdadeira: 'Verdadeira' };
const NOMES_ORIGEM = { botao: 'Botão do aparelho', painel: 'Painel web', nenhuma: '-' };

let pontos = [];
let sequencia = 0;
let versaoHistorico = -1;
let falhas = 0;
let limiteQuedaLivre = 2;
let limiteImpacto = 10;

document.querySelectorAll('.aba-btn').forEach(botao => {
  botao.onclick = () => {
    document.querySelectorAll('.aba-btn').forEach(b => b.classList.remove('ativa'));
    document.querySelectorAll('.aba').forEach(a => a.classList.remove('ativa'));
    botao.classList.add('ativa');
    $(botao.dataset.aba).classList.add('ativa');
    desenhar();
  };
});

async function enviarComando(acao) {
  try {
    await fetch('/api/comando?acao=' + acao, { cache: 'no-store' });
  } catch (erro) {}
}

$('bBuzzer').onclick = () => enviarComando('teste_buzzer');
$('bSimular').onclick = () => enviarComando('simular');
$('bCancelar').onclick = () => enviarComando('cancelar');

function definirSelo(id, classe, texto) {
  const selo = $(id);
  selo.className = 'selo ' + classe;
  selo.lastElementChild.textContent = texto;
}

function atualizarAlarme(d) {
  const tela = $('alarme');
  tela.hidden = !d.alerta;
  document.title = d.alerta ? 'QUEDA! - Decfall' : 'Decfall';
  if (!d.alerta) return;
  tela.classList.toggle('confirmada', d.confirmada);
  if (d.confirmada) {
    $('alarmeTitulo').textContent = 'Queda confirmada';
    $('alarmeTexto').textContent = 'Ninguém cancelou o alarme. Verifique a pessoa agora.';
    $('bCancelar').textContent = 'Desligar alarme';
  } else {
    $('alarmeTitulo').textContent = 'Possível queda detectada';
    $('alarmeTexto').textContent = 'Se foi alarme falso, desative agora. Em ' + d.restante + ' s a queda será marcada como verdadeira.';
    $('bCancelar').textContent = 'É erro: desativar alarme';
  }
}

function atualizarTela(d) {
  sequencia = d.seq;
  limiteQuedaLivre = d.lq;
  limiteImpacto = d.li;
  pontos = pontos.concat(d.amostras);
  if (pontos.length > MAX_PONTOS) pontos = pontos.slice(pontos.length - MAX_PONTOS);

  $('vTotal').textContent = d.acc.toFixed(1);
  $('vX').textContent = d.x.toFixed(1);
  $('vY').textContent = d.y.toFixed(1);
  $('vZ').textContent = d.z.toFixed(1);
  $('tQuedaLivre').textContent = limiteQuedaLivre;
  $('tImpacto').textContent = limiteImpacto;

  if (d.alerta) definirSelo('seloEstado', 'alarme', 'Alerta de queda');
  else if (!d.sensor) definirSelo('seloEstado', 'aviso', 'Sensor sem resposta');
  else definirSelo('seloEstado', 'ok', 'Monitorando');

  definirSelo('seloWifi', d.wifi ? 'ok' : 'aviso', d.wifi ? 'Wi-Fi ' + d.ip : 'Sem internet, sem Telegram');
  definirSelo('seloSensor', d.sensor ? 'ok' : 'aviso', d.sensor ? 'Acelerômetro ok' : 'Acelerômetro ausente');

  atualizarAlarme(d);

  if (d.hv !== versaoHistorico) {
    versaoHistorico = d.hv;
    carregarHistorico();
  }
  desenhar();
}

async function carregarHistorico() {
  try {
    const resposta = await fetch('/api/historico', { cache: 'no-store' });
    const h = await resposta.json();
    let verdadeiras = 0;
    let erros = 0;
    let linhas = '';
    h.itens.forEach(q => {
      if (q.status === 'verdadeira') verdadeiras++;
      if (q.status === 'erro') erros++;
      linhas += '<tr><td>' + q.id + '</td><td>' + q.hora + '</td><td>' + q.pico.toFixed(1) + '</td>';
      linhas += '<td><span class="marca m-' + q.status + '">' + NOMES_STATUS[q.status] + '</span>' + (q.simulada ? ' (teste)' : '') + '</td>';
      linhas += '<td>' + NOMES_ORIGEM[q.origem] + '</td></tr>';
    });
    $('corpo').innerHTML = linhas;
    $('vazio').hidden = h.itens.length > 0;
    $('rTotal').textContent = h.total;
    $('rVerdadeiras').textContent = verdadeiras;
    $('rErros').textContent = erros;
    $('contador').textContent = h.total > 0 ? '(' + h.total + ')' : '';
  } catch (erro) {}
}

function desenhar() {
  const tela = $('grafico');
  const largura = tela.clientWidth;
  const altura = tela.clientHeight;
  if (largura === 0) return;

  const escala = window.devicePixelRatio || 1;
  if (tela.width !== Math.round(largura * escala) || tela.height !== Math.round(altura * escala)) {
    tela.width = Math.round(largura * escala);
    tela.height = Math.round(altura * escala);
  }
  const g = tela.getContext('2d');
  g.setTransform(escala, 0, 0, escala, 0, 0);
  g.clearRect(0, 0, largura, altura);

  const margemEsq = 36;
  const margemDir = 6;
  const margemTopo = 8;
  const margemBase = 20;
  const areaLargura = largura - margemEsq - margemDir;
  const areaAltura = altura - margemTopo - margemBase;

  let topo = 30;
  const maior = Math.max(0, ...pontos);
  if (maior > topo) topo = Math.ceil(maior / 10) * 10;

  const posY = valor => margemTopo + areaAltura - (valor / topo) * areaAltura;
  const posX = indice => margemEsq + areaLargura - ((pontos.length - 1 - indice) / (MAX_PONTOS - 1)) * areaLargura;

  g.font = '11px system-ui, sans-serif';
  g.textBaseline = 'middle';
  g.textAlign = 'right';
  g.lineWidth = 1;
  for (let v = 0; v <= topo; v += 5) {
    const principal = v % 10 === 0;
    g.strokeStyle = principal ? '#b7cad3' : '#dfe9ee';
    g.beginPath();
    g.moveTo(margemEsq, posY(v));
    g.lineTo(largura - margemDir, posY(v));
    g.stroke();
    if (principal) {
      g.fillStyle = '#526673';
      g.fillText(v, margemEsq - 6, posY(v));
    }
  }
  for (let s = 0; s <= 10; s++) {
    const x = margemEsq + areaLargura - (s * PONTOS_POR_SEGUNDO / (MAX_PONTOS - 1)) * areaLargura;
    g.strokeStyle = '#dfe9ee';
    g.beginPath();
    g.moveTo(x, margemTopo);
    g.lineTo(x, margemTopo + areaAltura);
    g.stroke();
  }
  g.fillStyle = '#526673';
  g.textBaseline = 'alphabetic';
  g.textAlign = 'left';
  g.fillText('-10 s', margemEsq, altura - 5);
  g.textAlign = 'right';
  g.fillText('agora', largura - margemDir, altura - 5);

  g.lineWidth = 1.5;
  g.setLineDash([6, 4]);
  g.strokeStyle = '#e8a317';
  g.beginPath();
  g.moveTo(margemEsq, posY(limiteQuedaLivre));
  g.lineTo(largura - margemDir, posY(limiteQuedaLivre));
  g.stroke();
  g.strokeStyle = '#d7263d';
  g.beginPath();
  g.moveTo(margemEsq, posY(limiteImpacto));
  g.lineTo(largura - margemDir, posY(limiteImpacto));
  g.stroke();
  g.setLineDash([]);

  if (pontos.length < 2) return;
  g.strokeStyle = '#12232e';
  g.lineWidth = 1.6;
  g.lineJoin = 'round';
  g.beginPath();
  pontos.forEach((valor, indice) => {
    if (indice === 0) g.moveTo(posX(indice), posY(valor));
    else g.lineTo(posX(indice), posY(valor));
  });
  g.stroke();
}

async function ciclo() {
  try {
    const resposta = await fetch('/api/dados?desde=' + sequencia, { cache: 'no-store' });
    atualizarTela(await resposta.json());
    falhas = 0;
  } catch (erro) {
    falhas++;
    if (falhas >= 3) definirSelo('seloEstado', 'aviso', 'Sem conexão com o aparelho');
  }
  setTimeout(ciclo, 250);
}

window.addEventListener('resize', desenhar);
ciclo();
</script>
</body>
</html>)rawliteral";
