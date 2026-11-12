const $ = (id) => document.getElementById(id);
const historico = { x: [], y: [], z: [] };
const LIMITE = 60;

function definirIndicador(id, ligado) {
  const elemento = $(id);
  if (elemento) elemento.className = 'bolinha ' + (ligado ? 'ok' : 'off');
}

function desenharGrafico() {
  const canvas = $('grafico');
  if (!canvas) return;

  const ctx = canvas.getContext('2d');
  const larguraCSS = canvas.clientWidth;
  const alturaCSS = 230;
  const escala = window.devicePixelRatio || 1;

  canvas.width = larguraCSS * escala;
  canvas.height = alturaCSS * escala;
  ctx.setTransform(escala, 0, 0, escala, 0, 0);

  const largura = larguraCSS;
  const altura = alturaCSS;
  const margem = { esquerda: 48, direita: 12, topo: 12, baixo: 25 };
  const areaW = largura - margem.esquerda - margem.direita;
  const areaH = altura - margem.topo - margem.baixo;
  const todos = [...historico.x, ...historico.y, ...historico.z];

  if (!todos.length) {
    ctx.fillStyle = '#64748b';
    ctx.textAlign = 'center';
    ctx.fillText('Aguardando medições...', largura / 2, altura / 2);
    return;
  }

  let minimo = Math.min(...todos);
  let maximo = Math.max(...todos);
  if (minimo === maximo) { minimo -= 1; maximo += 1; }
  const folga = (maximo - minimo) * 0.12;
  minimo -= folga;
  maximo += folga;

  ctx.font = '11px Arial';
  ctx.strokeStyle = '#e5eaf2';
  ctx.fillStyle = '#64748b';
  ctx.lineWidth = 1;

  for (let i = 0; i <= 4; i++) {
    const y = margem.topo + areaH * i / 4;
    const valor = maximo - (maximo - minimo) * i / 4;
    ctx.beginPath();
    ctx.moveTo(margem.esquerda, y);
    ctx.lineTo(largura - margem.direita, y);
    ctx.stroke();
    ctx.textAlign = 'right';
    ctx.fillText(Math.round(valor), margem.esquerda - 7, y + 4);
  }

  [
    { chave: 'x', cor: '#2563eb' },
    { chave: 'y', cor: '#16a34a' },
    { chave: 'z', cor: '#ea580c' }
  ].forEach((serie) => {
    const valores = historico[serie.chave];
    if (!valores.length) return;

    ctx.beginPath();
    ctx.strokeStyle = serie.cor;
    ctx.lineWidth = 2;
    valores.forEach((valor, indice) => {
      const x = margem.esquerda + (valores.length === 1 ? 0 : indice * areaW / (LIMITE - 1));
      const y = margem.topo + ((maximo - valor) / (maximo - minimo)) * areaH;
      if (indice === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    });
    ctx.stroke();
  });
}

function adicionarHistorico(dados) {
  ['x', 'y', 'z'].forEach((eixo) => {
    historico[eixo].push(Number(dados[eixo]));
    if (historico[eixo].length > LIMITE) historico[eixo].shift();
  });
  desenharGrafico();
}

async function atualizarDados() {
  if (!$('grafico')) return;

  try {
    const resposta = await fetch('/dados');
    if (!resposta.ok) throw new Error('HTTP ' + resposta.status);
    const dados = await resposta.json();

    $('x').textContent = dados.x;
    $('y').textContent = dados.y;
    $('z').textContent = dados.z;
    $('wifi').textContent = dados.wifi ? 'Conectado' : 'Desconectado';
    $('sensor').textContent = dados.sensor ? 'Detectado' : 'Não detectado';
    $('buzzer').textContent = dados.buzzer ? 'Ligado' : 'Desligado';

    definirIndicador('wifi-bolinha', dados.wifi);
    definirIndicador('sensor-bolinha', dados.sensor);
    definirIndicador('buzzer-bolinha', dados.buzzer);

    $('status').textContent = dados.status;
    $('cartao-alerta').classList.toggle('alerta', dados.alerta);
    $('selo-alerta').textContent = dados.alerta ? 'ALERTA' : 'NORMAL';

    adicionarHistorico(dados);
    $('atualizacao').textContent = 'Última atualização: ' + new Date().toLocaleTimeString('pt-BR');
  } catch (erro) {
    $('wifi').textContent = 'Sem comunicação';
    $('atualizacao').textContent = 'Não foi possível consultar o ESP32.';
    definirIndicador('wifi-bolinha', false);
    console.error(erro);
  }
}

async function enviarComando(acao) {
  const botao = acao === 'teste_buzzer' ? $('btn-teste') : $('btn-reset');
  if (!botao) return;

  botao.disabled = true;
  $('mensagem-comando').textContent = 'Enviando comando...';

  try {
    const resposta = await fetch('/comando?acao=' + encodeURIComponent(acao));
    const resultado = await resposta.json();
    if (!resposta.ok || !resultado.ok) throw new Error(resultado.mensagem || 'Falha no comando');
    $('mensagem-comando').textContent = resultado.mensagem;
    atualizarDados();
  } catch (erro) {
    $('mensagem-comando').textContent = 'Não foi possível enviar o comando.';
    console.error(erro);
  } finally {
    botao.disabled = false;
  }
}

async function carregarLogs() {
  if (!$('lista-logs')) return;

  try {
    const resposta = await fetch('/api/logs');
    if (!resposta.ok) throw new Error('HTTP ' + resposta.status);
    $('lista-logs').textContent = await resposta.text();
    $('mensagem-logs').textContent = 'Histórico carregado do ESP32.';
  } catch (erro) {
    $('lista-logs').textContent = 'Não foi possível carregar os logs.';
    $('mensagem-logs').textContent = 'Verifique a conexão com o dispositivo.';
    console.error(erro);
  }
}

if ($('btn-teste')) $('btn-teste').addEventListener('click', () => enviarComando('teste_buzzer'));
if ($('btn-reset')) $('btn-reset').addEventListener('click', () => enviarComando('silenciar'));
if ($('atualizar')) $('atualizar').addEventListener('click', carregarLogs);

window.addEventListener('resize', desenharGrafico);

if ($('grafico')) {
  desenharGrafico();
  atualizarDados();
  setInterval(atualizarDados, 500);
}

if ($('lista-logs')) carregarLogs();
