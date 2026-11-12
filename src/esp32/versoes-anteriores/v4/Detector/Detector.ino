#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>
#include <time.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include "pagina.h"

#define WIFI_SSID "SEU_WIFI"
#define WIFI_PASSWORD "SUA_SENHA"
#define AP_SSID "DECFALL"
#define AP_SENHA "SENHA_AP"
#define BOT_TOKEN "SEU_TOKEN"
#define CHAT_ID "SEU_CHAT_ID"

#define SDA_PIN 21
#define SCL_PIN 22
#define BOTAO_PIN 13
#define BUZZER_PIN 25
#define MPU_ADDR 0x68
#define OLED_ADDR 0x3C
#define LARGURA 128
#define ALTURA 32

#define TAM_BUFFER 256
#define MAX_HISTORICO 20
#define TAM_FILA 4

const float LIMITE_QUEDA_LIVRE = 2.0;
const float LIMITE_IMPACTO = 45.0;
const unsigned long TEMPO_QUEDA_LIVRE_MIN = 60;
const unsigned long JANELA_IMPACTO = 1000;
const unsigned long TEMPO_CONFIRMACAO = 30000;
const unsigned long INTERVALO_AMOSTRA = 20;
const float ESCALA = 9.80665 / 4096.0;

enum StatusQueda { PENDENTE, ERRO, VERDADEIRA };
enum Origem { NENHUMA, BOTAO, PAINEL };

struct Queda {
  int id;
  char hora[24];
  float pico;
  StatusQueda status;
  Origem origem;
  bool simulada;
};

WiFiClientSecure clienteSeguro;
UniversalTelegramBot bot(BOT_TOKEN, clienteSeguro);
WebServer servidor(80);
Adafruit_SSD1306 display(LARGURA, ALTURA, &Wire, -1);

int16_t ax = 0;
int16_t ay = 0;
int16_t az = 0;
float aceleracao = 0;

float bufferAcel[TAM_BUFFER];
unsigned long sequenciaAmostra = 0;

Queda historico[MAX_HISTORICO];
int quantidadeHistorico = 0;
int totalQuedas = 0;
int versaoHistorico = 0;

String filaTelegram[TAM_FILA];
int quantidadeFila = 0;

bool mpuEncontrado = false;
bool oledEncontrado = false;
bool alertaAtivo = false;

unsigned long inicioAlerta = 0;
unsigned long ignorarAte = 0;
unsigned long testeBuzzerAte = 0;
unsigned long ultimaAmostra = 0;
unsigned long ultimoDisplay = 0;
unsigned long ultimaTentativaMPU = 0;
int falhasLeitura = 0;

bool horaValida() {
  return time(nullptr) > 1700000000;
}

String carimboDeTempo() {
  struct tm info;
  if (horaValida() && getLocalTime(&info, 0)) {
    char texto[24];
    strftime(texto, sizeof(texto), "%d/%m/%Y %H:%M:%S", &info);
    return String(texto);
  }
  return "t+" + String(millis() / 1000) + "s";
}

void registrarLog(String mensagem) {
  Serial.println("[" + carimboDeTempo() + "] " + mensagem);
}

void enfileirarTelegram(String mensagem) {
  if (quantidadeFila < TAM_FILA) {
    filaTelegram[quantidadeFila] = mensagem;
    quantidadeFila++;
  }
}

void processarTelegram() {
  if (quantidadeFila == 0) return;

  String mensagem = filaTelegram[0];
  for (int i = 1; i < quantidadeFila; i++) {
    filaTelegram[i - 1] = filaTelegram[i];
  }
  quantidadeFila--;

  if (WiFi.status() != WL_CONNECTED) {
    registrarLog("Telegram nao enviado: sem Wi-Fi");
    return;
  }

  if (horaValida()) {
    clienteSeguro.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  } else {
    clienteSeguro.setInsecure();
  }

  bool enviou = bot.sendMessage(CHAT_ID, mensagem, "");
  if (enviou) {
    registrarLog("Mensagem enviada ao Telegram");
  } else {
    registrarLog("Falha ao enviar mensagem ao Telegram");
  }
}

long segundosRestantes() {
  long restante = (long)TEMPO_CONFIRMACAO - (long)(millis() - inicioAlerta);
  if (restante <= 0) return 0;
  return (restante + 999) / 1000;
}

void dispararAlerta(float pico, bool simulada) {
  for (int i = MAX_HISTORICO - 1; i > 0; i--) {
    historico[i] = historico[i - 1];
  }
  if (quantidadeHistorico < MAX_HISTORICO) quantidadeHistorico++;
  totalQuedas++;
  versaoHistorico++;

  historico[0].id = totalQuedas;
  carimboDeTempo().toCharArray(historico[0].hora, sizeof(historico[0].hora));
  historico[0].pico = pico;
  historico[0].status = PENDENTE;
  historico[0].origem = NENHUMA;
  historico[0].simulada = simulada;

  alertaAtivo = true;
  inicioAlerta = millis();
  registrarLog("ALERTA: possivel queda #" + String(totalQuedas) + ", pico " + String(pico, 1) + " m/s2");

  String mensagem = "🚨 DECFALL: possível queda detectada!";
  if (simulada) mensagem += " (teste)";
  mensagem += "\nPico: " + String(pico, 1) + " m/s²";
  mensagem += "\nSe foi alarme falso, aperte o botão do aparelho em até " + String(TEMPO_CONFIRMACAO / 1000) + " s.";
  if (WiFi.status() == WL_CONNECTED) {
    mensagem += "\nPainel: http://" + WiFi.localIP().toString();
  }
  enfileirarTelegram(mensagem);
}

void cancelarAlerta(Origem origem) {
  if (!alertaAtivo) return;

  alertaAtivo = false;
  ignorarAte = millis() + 2000;
  versaoHistorico++;
  historico[0].origem = origem;

  String quem = (origem == BOTAO) ? "botão do aparelho" : "painel web";

  if (historico[0].status == PENDENTE) {
    historico[0].status = ERRO;
    registrarLog("Queda #" + String(historico[0].id) + " anulada: alarme falso");
    enfileirarTelegram("✅ DECFALL: alarme falso. A queda #" + String(historico[0].id) + " foi anulada pelo " + quem + ".");
  } else {
    registrarLog("Alarme da queda #" + String(historico[0].id) + " desligado pelo " + quem);
    enfileirarTelegram("ℹ️ DECFALL: o alarme da queda #" + String(historico[0].id) + " foi desligado pelo " + quem + ".");
  }
}

void verificarConfirmacao() {
  if (!alertaAtivo) return;
  if (historico[0].status != PENDENTE) return;
  if (millis() - inicioAlerta < TEMPO_CONFIRMACAO) return;

  historico[0].status = VERDADEIRA;
  versaoHistorico++;
  registrarLog("Queda #" + String(historico[0].id) + " CONFIRMADA: ninguem cancelou");
  enfileirarTelegram("🆘 DECFALL: queda #" + String(historico[0].id) + " CONFIRMADA! Ninguém cancelou o alarme em " + String(TEMPO_CONFIRMACAO / 1000) + " s. Verifique a pessoa agora.");
}

void detectarQueda(float a) {
  static bool emQuedaLivre = false;
  static bool esperandoImpacto = false;
  static unsigned long inicioQuedaLivre = 0;
  static unsigned long fimQuedaLivre = 0;

  unsigned long agora = millis();

  if (alertaAtivo || agora < ignorarAte) {
    emQuedaLivre = false;
    esperandoImpacto = false;
    return;
  }

  if (!esperandoImpacto) {
    if (a < LIMITE_QUEDA_LIVRE) {
      if (!emQuedaLivre) {
        emQuedaLivre = true;
        inicioQuedaLivre = agora;
      }
      if (agora - inicioQuedaLivre >= TEMPO_QUEDA_LIVRE_MIN) {
        esperandoImpacto = true;
        fimQuedaLivre = agora;
      }
    } else {
      emQuedaLivre = false;
    }
    return;
  }

  if (a < LIMITE_QUEDA_LIVRE) {
    fimQuedaLivre = agora;
  }

  if (a > LIMITE_IMPACTO) {
    esperandoImpacto = false;
    emQuedaLivre = false;
    dispararAlerta(a, false);
  } else if (agora - fimQuedaLivre > JANELA_IMPACTO) {
    esperandoImpacto = false;
    emQuedaLivre = false;
  }
}

bool iniciarMPU() {
  Wire.beginTransmission(MPU_ADDR);
  if (Wire.endTransmission() != 0) return false;

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission();

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C);
  Wire.write(0x10);
  Wire.endTransmission();

  delay(100);
  return true;
}

int16_t lerInteiro16() {
  int alto = Wire.read();
  int baixo = Wire.read();
  return (int16_t)((alto << 8) | baixo);
}

bool lerAcelerometro() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)6) != 6) return false;

  ax = lerInteiro16();
  ay = lerInteiro16();
  az = lerInteiro16();
  return true;
}

void amostrar() {
  if (!mpuEncontrado) {
    if (millis() - ultimaTentativaMPU >= 2000) {
      ultimaTentativaMPU = millis();
      mpuEncontrado = iniciarMPU();
      if (mpuEncontrado) registrarLog("Acelerometro encontrado");
    }
    return;
  }

  if (millis() - ultimaAmostra < INTERVALO_AMOSTRA) return;
  ultimaAmostra = millis();

  if (!lerAcelerometro()) {
    falhasLeitura++;
    if (falhasLeitura >= 25) {
      mpuEncontrado = false;
      registrarLog("Acelerometro parou de responder");
    }
    return;
  }
  falhasLeitura = 0;

  aceleracao = sqrt((float)ax * ax + (float)ay * ay + (float)az * az) * ESCALA;

  for (int i = 0; i < TAM_BUFFER - 1; i++) {
    bufferAcel[i] = bufferAcel[i + 1];
  }
  bufferAcel[TAM_BUFFER - 1] = aceleracao;
  sequenciaAmostra++;

  detectarQueda(aceleracao);
}

void verificarBotao() {
  static int ultimaLeitura = HIGH;
  static int estadoEstavel = HIGH;
  static unsigned long tempoMudanca = 0;

  int leitura = digitalRead(BOTAO_PIN);

  if (leitura != ultimaLeitura) {
    tempoMudanca = millis();
  }

  if (millis() - tempoMudanca > 50 && leitura != estadoEstavel) {
    estadoEstavel = leitura;
    if (estadoEstavel == LOW) {
      if (alertaAtivo) {
        cancelarAlerta(BOTAO);
      } else {
        testeBuzzerAte = millis() + 150;
      }
    }
  }

  ultimaLeitura = leitura;
}

void atualizarBuzzer() {
  bool apitoAlerta = alertaAtivo && ((millis() / 250) % 2 == 0);
  bool apitoTeste = millis() < testeBuzzerAte;
  digitalWrite(BUZZER_PIN, (apitoAlerta || apitoTeste) ? HIGH : LOW);
}

void escreverDisplay(String linha1, String linha2, String linha3) {
  if (!oledEncontrado) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(linha1);
  display.setCursor(0, 11);
  display.print(linha2);
  display.setCursor(0, 22);
  display.print(linha3);
  display.display();
}

void atualizarDisplay() {
  if (alertaAtivo) {
    String terceira = "QUEDA CONFIRMADA!";
    if (historico[0].status == PENDENTE) {
      terceira = "Confirma em " + String(segundosRestantes()) + " s";
    }
    escreverDisplay("!! POSSIVEL QUEDA !!", "Botao = falso alarme", terceira);
    return;
  }

  String linha1 = "ERRO NO MPU6050";
  String linha2 = "Verifique SDA e SCL";
  if (mpuEncontrado) {
    linha1 = "MONITORANDO";
    linha2 = "Acel: " + String(aceleracao, 1) + " m/s2";
  }

  String linha3 = "AP: 192.168.4.1";
  if (WiFi.status() == WL_CONNECTED) {
    linha3 = WiFi.localIP().toString();
  }
  escreverDisplay(linha1, linha2, linha3);
}

String textoStatus(StatusQueda status) {
  if (status == PENDENTE) return "pendente";
  if (status == ERRO) return "erro";
  return "verdadeira";
}

String textoOrigem(Origem origem) {
  if (origem == BOTAO) return "botao";
  if (origem == PAINEL) return "painel";
  return "nenhuma";
}

String textoBooleano(bool valor) {
  if (valor) return "true";
  return "false";
}

void enviarJson(String json) {
  servidor.sendHeader("Cache-Control", "no-store");
  servidor.send(200, "application/json; charset=utf-8", json);
}

void rotaInicio() {
  servidor.send_P(200, "text/html; charset=utf-8", PAGINA_HTML);
}

void rotaDados() {
  unsigned long desde = servidor.arg("desde").toInt();
  if (desde > sequenciaAmostra) desde = sequenciaAmostra;
  unsigned long novas = sequenciaAmostra - desde;
  if (novas > TAM_BUFFER) novas = TAM_BUFFER;

  bool confirmada = alertaAtivo && historico[0].status == VERDADEIRA;
  long restante = 0;
  if (alertaAtivo && !confirmada) restante = segundosRestantes();

  String json = "{";
  json += "\"seq\":" + String(sequenciaAmostra);
  json += ",\"acc\":" + String(aceleracao, 1);
  json += ",\"x\":" + String(ax * ESCALA, 1);
  json += ",\"y\":" + String(ay * ESCALA, 1);
  json += ",\"z\":" + String(az * ESCALA, 1);
  json += ",\"alerta\":" + textoBooleano(alertaAtivo);
  json += ",\"confirmada\":" + textoBooleano(confirmada);
  json += ",\"restante\":" + String(restante);
  json += ",\"sensor\":" + textoBooleano(mpuEncontrado);
  json += ",\"wifi\":" + textoBooleano(WiFi.status() == WL_CONNECTED);
  json += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += ",\"hv\":" + String(versaoHistorico);
  json += ",\"lq\":" + String(LIMITE_QUEDA_LIVRE, 1);
  json += ",\"li\":" + String(LIMITE_IMPACTO, 1);
  json += ",\"amostras\":[";
  for (int i = TAM_BUFFER - (int)novas; i < TAM_BUFFER; i++) {
    if (i > TAM_BUFFER - (int)novas) json += ",";
    json += String(bufferAcel[i], 1);
  }
  json += "]}";

  enviarJson(json);
}

void rotaHistorico() {
  String json = "{\"total\":" + String(totalQuedas) + ",\"itens\":[";
  for (int i = 0; i < quantidadeHistorico; i++) {
    if (i > 0) json += ",";
    json += "{\"id\":" + String(historico[i].id);
    json += ",\"hora\":\"" + String(historico[i].hora) + "\"";
    json += ",\"pico\":" + String(historico[i].pico, 1);
    json += ",\"status\":\"" + textoStatus(historico[i].status) + "\"";
    json += ",\"origem\":\"" + textoOrigem(historico[i].origem) + "\"";
    json += ",\"simulada\":" + textoBooleano(historico[i].simulada) + "}";
  }
  json += "]}";

  enviarJson(json);
}

void rotaComando() {
  String acao = servidor.arg("acao");

  if (acao == "cancelar") {
    cancelarAlerta(PAINEL);
  } else if (acao == "teste_buzzer") {
    testeBuzzerAte = millis() + 1000;
    registrarLog("Teste do buzzer pelo painel");
  } else if (acao == "simular") {
    if (!alertaAtivo) dispararAlerta(25.0, true);
  } else {
    servidor.send(400, "application/json; charset=utf-8", "{\"ok\":false}");
    return;
  }

  enviarJson("{\"ok\":true}");
}

void rotaNaoEncontrada() {
  servidor.send(404, "text/plain; charset=utf-8", "Pagina nao encontrada");
}

void configurarRotas() {
  servidor.on("/", HTTP_GET, rotaInicio);
  servidor.on("/api/dados", HTTP_GET, rotaDados);
  servidor.on("/api/historico", HTTP_GET, rotaHistorico);
  servidor.on("/api/comando", HTTP_GET, rotaComando);
  servidor.onNotFound(rotaNaoEncontrada);
}

void setup() {
  Serial.begin(115200);

  pinMode(BOTAO_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  mpuEncontrado = iniciarMPU();
  if (mpuEncontrado) {
    Serial.println("Acelerometro encontrado!");
  } else {
    Serial.println("Acelerometro nao encontrado! Verifique SDA, SCL, VCC e GND.");
  }

  oledEncontrado = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (!oledEncontrado) Serial.println("OLED nao encontrado!");
  escreverDisplay("DECFALL", "Detector de quedas", "Iniciando...");
  delay(1500);

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_SENHA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.google.com");

  escreverDisplay("Conectando Wi-Fi...", WIFI_SSID, "");
  unsigned long inicioWifi = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicioWifi < 15000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  String mensagemInicial = "📟 DECFALL ligado.";
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wi-Fi conectado! IP: ");
    Serial.println(WiFi.localIP());
    mensagemInicial += "\nPainel: http://" + WiFi.localIP().toString();

    unsigned long inicioNtp = millis();
    while (!horaValida() && millis() - inicioNtp < 8000) {
      delay(200);
    }
  } else {
    Serial.println("Wi-Fi nao conectou. Use o Wi-Fi DECFALL e abra 192.168.4.1");
  }
  enfileirarTelegram(mensagemInicial);

  configurarRotas();
  servidor.begin();
  registrarLog("Sistema iniciado");
}

void loop() {
  amostrar();
  verificarBotao();
  verificarConfirmacao();
  atualizarBuzzer();
  processarTelegram();

  if (millis() - ultimoDisplay >= 250) {
    ultimoDisplay = millis();
    atualizarDisplay();
  }

  servidor.handleClient();
  delay(1);
}
