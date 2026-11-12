#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>
#include <time.h>
#include <LittleFS.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// =====================================================
// CONFIGURAÇÕES DO WI-FI E TELEGRAM
// =====================================================
#define WIFI_SSID "SEU_WIFI"
#define WIFI_PASSWORD "SUA_SENHA"

// Telegram BOT Token (BotFather)
#define BOT_TOKEN "SEU_TOKEN"
// Use @myidbot (IDBot) para saber qual o seu ID
#define CHAT_ID "SEU_CHAT_ID"

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
WebServer server(80);

// =====================================================
// PINOS
// =====================================================
#define SDA_PIN 21
#define SCL_PIN 22
#define BOTAO_PIN 13
#define BUZZER_PIN 25

#define MPU_ADDR 0x68
#define OLED_ADDR 0x3C

// =====================================================
// OLED 128x32
// =====================================================
#define LARGURA 128
#define ALTURA 32
Adafruit_SSD1306 display(LARGURA, ALTURA, &Wire, -1);

// =====================================================
// VARIAVEIS
// =====================================================
int16_t ax = 0;
int16_t ay = 0;
int16_t az = 0;

float aceleracao = 0;

bool alertaQueda = false;
bool quedaLivre = false;
bool mpuEncontrado = false;
bool oledEncontrado = false;
bool buzzerLigado = false;
bool telegramPendente = false;
bool horaSincronizada = false;

unsigned long tempoQueda = 0;
unsigned long ultimoDisplay = 0;
unsigned long testeBuzzerAte = 0;

// MPU6050 configurado em +-8g => 4096 LSB/g
// Limites iniciais para testes (~0,53 g e ~2,65 g); precisam ser calibrados.
const float LIMITE_QUEDA = 2168;
const float LIMITE_IMPACTO = 10854;

const char* ARQUIVO_LOGS = "/logs.txt";
const size_t TAMANHO_MAX_LOGS = 6000;

// =====================================================
// LOGS (gravados no LittleFS)
// =====================================================
void registrarLog(const String& mensagem) {
  String carimbo;
  struct tm info;
  if (horaSincronizada && getLocalTime(&info, 0)) {
    char buf[24];
    strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &info);
    carimbo = String(buf);
  } else {
    carimbo = "t+" + String(millis() / 1000) + "s";
  }

  String linha = "[" + carimbo + "] " + mensagem;
  Serial.println(linha);

  if (LittleFS.exists(ARQUIVO_LOGS)) {
    File f = LittleFS.open(ARQUIVO_LOGS, "r");
    size_t tamanho = f ? f.size() : 0;
    if (f) f.close();
    if (tamanho > TAMANHO_MAX_LOGS) LittleFS.remove(ARQUIVO_LOGS);
  }

  File f = LittleFS.open(ARQUIVO_LOGS, "a");
  if (f) {
    f.println(linha);
    f.close();
  }
}

// =====================================================
// LEITURA DO MPU
// =====================================================
int16_t ler16(uint8_t registrador) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(registrador);
  Wire.endTransmission(false);

  Wire.requestFrom(MPU_ADDR, (uint8_t)2);

  int16_t valor = 0;
  if (Wire.available() >= 2) {
    valor = (int16_t)((Wire.read() << 8) | Wire.read());
  }
  return valor;
}

// =====================================================
// INICIALIZAR MPU
// =====================================================
void iniciarMPU() {
  // Acorda o sensor
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission();

  // Faixa do acelerometro: +-8g
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C);
  Wire.write(0x10);
  Wire.endTransmission();

  delay(100);
}

// =====================================================
// DETECCAO SIMPLES DE POSSIVEL QUEDA
// =====================================================
void detectarQueda() {
  aceleracao = sqrt(
    (float)ax * ax +
    (float)ay * ay +
    (float)az * az
  );

  if (aceleracao < LIMITE_QUEDA && !alertaQueda && !quedaLivre) {
    quedaLivre = true;
    tempoQueda = millis();
  }

  if (quedaLivre && !alertaQueda) {
    if (millis() - tempoQueda <= 1500) {
      if (aceleracao > LIMITE_IMPACTO) {
        alertaQueda = true;
        telegramPendente = true;
        registrarLog("ALERTA: possivel queda detectada");
      }
    } else {
      quedaLivre = false;
    }
  }
}

// =====================================================
// BOTAO COM DEBOUNCE
// =====================================================
void verificarBotao() {
  static int ultimaLeitura = HIGH;
  static int estadoEstavel = HIGH;
  static unsigned long tempoMudanca = 0;

  int leitura = digitalRead(BOTAO_PIN);

  if (leitura != ultimaLeitura) {
    tempoMudanca = millis();
  }

  if (millis() - tempoMudanca > 50) {
    if (leitura != estadoEstavel) {
      estadoEstavel = leitura;

      // INPUT_PULLUP: LOW significa botao pressionado.
      if (estadoEstavel == LOW && alertaQueda) {
        alertaQueda = false;
        quedaLivre = false;
        registrarLog("Alerta cancelado pelo botao");
      }
    }
  }

  ultimaLeitura = leitura;
}

// =====================================================
// TELEGRAM
// =====================================================
void enviarTelegramSePendente() {
  if (!telegramPendente) return;
  telegramPendente = false;

  if (WiFi.status() != WL_CONNECTED) {
    registrarLog("Telegram nao enviado: sem Wi-Fi");
    return;
  }

  bool ok = bot.sendMessage(CHAT_ID, "ALERTA: possivel queda detectada!", "");
  registrarLog(ok ? "Mensagem enviada ao Telegram" : "Falha ao enviar mensagem ao Telegram");
}

// =====================================================
// ROTAS DA PAGINA WEB - ARQUIVOS NO LITTLEFS
// =====================================================
void enviarArquivo(const char* caminho, const char* tipo) {
  File arquivo = LittleFS.open(caminho, "r");
  if (!arquivo) {
    server.send(404, "text/plain; charset=utf-8", String(caminho) + " nao encontrado no LittleFS");
    return;
  }
  server.streamFile(arquivo, tipo);
  arquivo.close();
}

void configurarRotas() {
  server.on("/", HTTP_GET, []() { enviarArquivo("/index.html", "text/html"); });
  server.on("/logs", HTTP_GET, []() { enviarArquivo("/logs.html", "text/html"); });
  server.on("/style.css", HTTP_GET, []() { enviarArquivo("/style.css", "text/css"); });
  server.on("/app.js", HTTP_GET, []() { enviarArquivo("/app.js", "application/javascript"); });

  server.on("/dados", HTTP_GET, []() {
    String status = alertaQueda
      ? "ALERTA: POSSÍVEL QUEDA DETECTADA!"
      : "Monitorando";

    String json = "{";
    json += "\"x\":" + String(ax) + ",";
    json += "\"y\":" + String(ay) + ",";
    json += "\"z\":" + String(az) + ",";
    json += "\"wifi\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
    json += "\"sensor\":" + String(mpuEncontrado ? "true" : "false") + ",";
    json += "\"buzzer\":" + String(buzzerLigado ? "true" : "false") + ",";
    json += "\"alerta\":" + String(alertaQueda ? "true" : "false") + ",";
    json += "\"status\":\"" + status + "\"";
    json += "}";

    server.send(200, "application/json; charset=utf-8", json);
  });

  server.on("/api/logs", HTTP_GET, []() {
    File f = LittleFS.open(ARQUIVO_LOGS, "r");
    if (!f) {
      server.send(200, "text/plain; charset=utf-8", "Nenhum registro ainda.");
      return;
    }
    server.streamFile(f, "text/plain; charset=utf-8");
    f.close();
  });

  server.on("/comando", HTTP_GET, []() {
    String acao = server.arg("acao");

    if (acao == "teste_buzzer") {
      testeBuzzerAte = millis() + 1000;
      registrarLog("Teste do buzzer pelo painel");
      server.send(200, "application/json; charset=utf-8",
                  "{\"ok\":true,\"mensagem\":\"Buzzer testado.\"}");
    } else if (acao == "silenciar") {
      alertaQueda = false;
      quedaLivre = false;
      testeBuzzerAte = 0;
      registrarLog("Alerta silenciado pelo painel");
      server.send(200, "application/json; charset=utf-8",
                  "{\"ok\":true,\"mensagem\":\"Alerta silenciado.\"}");
    } else {
      server.send(400, "application/json; charset=utf-8",
                  "{\"ok\":false,\"mensagem\":\"Comando desconhecido.\"}");
    }
  });

  server.onNotFound([]() {
    server.send(404, "text/plain; charset=utf-8", "Pagina nao encontrada");
  });
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);

  pinMode(BOTAO_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // I2C
  Wire.begin(SDA_PIN, SCL_PIN);

  // MPU
  Wire.beginTransmission(MPU_ADDR);
  mpuEncontrado = (Wire.endTransmission() == 0);

  if (mpuEncontrado) {
    Serial.println("MPU encontrado!");
    iniciarMPU();
  } else {
    Serial.println("MPU nao encontrado! Verifique SDA, SCL, VCC e GND.");
  }

  // OLED (o sistema continua funcionando sem ele)
  oledEncontrado = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (oledEncontrado) {
    Serial.println("OLED encontrado!");
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("Detector");
    display.println("de Quedas");
    display.display();
    delay(2000);
  } else {
    Serial.println("OLED nao encontrado!");
  }

  // LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("Erro ao iniciar LittleFS!");
    while (true) {
      delay(100);
    }
  }
  Serial.println("LittleFS iniciado!");

  // Wi-Fi
  Serial.print("Conectando ao Wi-Fi: ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Certificado seguro para o Telegram
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  unsigned long inicioWifi = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicioWifi < 20000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    // Sincronizacao de horario (necessaria para o Telegram HTTPS), fuso -03:00
    Serial.print("Buscando horario (NTP): ");
    configTime(-3 * 3600, 0, "pool.ntp.org");
    unsigned long inicioNtp = millis();
    time_t agora = time(nullptr);
    while (agora < 24 * 3600 && millis() - inicioNtp < 10000) {
      Serial.print(".");
      delay(200);
      agora = time(nullptr);
    }
    horaSincronizada = (agora >= 24 * 3600);
    Serial.println(horaSincronizada ? " ok" : " falhou");
  } else {
    Serial.println("Wi-Fi nao conectou (tentando em segundo plano).");
  }

  // Servidor HTTP
  configurarRotas();
  server.begin();
  Serial.println("Servidor HTTP iniciado!");

  registrarLog("Sistema iniciado");

  if (WiFi.status() == WL_CONNECTED) {
    bool ok = bot.sendMessage(CHAT_ID, "Bot iniciou com sucesso!", "");
    registrarLog(ok ? "Mensagem inicial enviada ao Telegram" : "Falha na mensagem inicial do Telegram");
  }
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  if (mpuEncontrado) {
    ax = ler16(0x3B);
    ay = ler16(0x3D);
    az = ler16(0x3F);

    detectarQueda();
  }

  verificarBotao();

  buzzerLigado = alertaQueda || (millis() < testeBuzzerAte);
  digitalWrite(BUZZER_PIN, buzzerLigado ? HIGH : LOW);

  enviarTelegramSePendente();

  // Atualiza OLED a cada 200 ms
  if (oledEncontrado && millis() - ultimoDisplay >= 200) {
    ultimoDisplay = millis();

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);

    if (alertaQueda) {
      display.setCursor(0, 0);
      display.println("!!! ALERTA !!!");
      display.setCursor(0, 12);
      display.println("POSSIVEL QUEDA");
      display.setCursor(0, 23);
      display.println("Pressione RESET");
    } else {
      display.setCursor(0, 0);
      display.println(mpuEncontrado ? "MONITORANDO" : "ERRO NO MPU6050");

      display.setCursor(0, 11);
      display.print("X:");
      display.print(ax);
      display.print(" Y:");
      display.print(ay);

      display.setCursor(0, 22);
      display.print("Z:");
      display.println(az);
    }

    display.display();
  }

  server.handleClient();
  delay(20);
}
