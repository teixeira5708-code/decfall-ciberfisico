#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>
#include <LittleFS.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
// CONFIGURAÇÕES DO WI-FI E TELEGRAM
// =====================================================
#define WIFI_SSID "SEU_WIFI"
#define WIFI_PASSWORD "SUA_SENHA"


// Telegram BOT Token (Botfather)
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

unsigned long tempoQueda = 0;
unsigned long ultimoDisplay = 0;

// Limites iniciais para testes; precisam ser calibrados.
const float LIMITE_QUEDA = 8670;
const float LIMITE_IMPACTO = 43350;

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
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);
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
        Serial.println("ALERTA: POSSIVEL QUEDA!");
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
        digitalWrite(BUZZER_PIN, LOW);
        Serial.println("Alerta cancelado pelo botao!");
      }
    }
  }

  ultimaLeitura = leitura;
}

// =====================================================
// ROTAS DA PAGINA WEB - ARQUIVOS NO LITTLEFS
// =====================================================
void configurarRotas() {
  server.on("/", HTTP_GET, []() {
    File arquivo = LittleFS.open("/index.html", "r");
    if (!arquivo) {
      server.send(404, "text/plain; charset=utf-8", "index.html nao encontrado no LittleFS");
      return;
    }
    server.streamFile(arquivo, "text/html");
    arquivo.close();
  });

  server.on("/style.css", HTTP_GET, []() {
    File arquivo = LittleFS.open("/style.css", "r");
    if (!arquivo) {
      server.send(404, "text/plain; charset=utf-8", "style.css nao encontrado no LittleFS");
      return;
    }
    server.streamFile(arquivo, "text/css");
    arquivo.close();
  });

  server.on("/script.js", HTTP_GET, []() {
    File arquivo = LittleFS.open("/script.js", "r");
    if (!arquivo) {
      server.send(404, "text/plain; charset=utf-8", "script.js nao encontrado no LittleFS");
      return;
    }
    server.streamFile(arquivo, "application/javascript");
    arquivo.close();
  });

  server.on("/dados", HTTP_GET, []() {
    String status = alertaQueda
      ? "ALERTA: POSSIVEL QUEDA DETECTADA!"
      : "Monitorando";

    String json = "{";
    json += "\"x\":" + String(ax) + ",";
    json += "\"y\":" + String(ay) + ",";
    json += "\"z\":" + String(az) + ",";
    json += "\"alerta\":" + String(alertaQueda ? "true" : "false") + ",";
    json += "\"status\":\"" + status + "\"";
    json += "}";

    server.send(200, "application/json", json);
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

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED nao encontrado!");
    while (true) {
      delay(100);
    }
  }

  Serial.println("OLED encontrado!");
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println("Detector");
  display.println("de Quedas");
  display.display();
  delay(2000);

  // LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("Erro ao iniciar LittleFS!");
    while (true) {
      delay(100);
    }
  }
  Serial.println("LittleFS iniciado!");

  // Conexão Wi-Fi
  Serial.print("Conectando ao Wi-Fi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  // Configuração do certificado seguro para o Telegram
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Servidor HTTP
  configurarRotas();
  server.begin();
  Serial.println("Servidor HTTP iniciado!");
}

// Configuração do certificado seguro para o Telegram
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);  

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Sincronização de tempo necessária para o Telegram HTTPS
  Serial.print("Buscando horário (NTP): ");
  configTime(0, 0, "pool.ntp.org");
  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);

  // Servidor Web local
  server.on("/", paginaInicial);
  server.on("/dados", enviarDados);
  server.begin();
  Serial.println("Servidor HTTP iniciado!");

  // Envio da mensagem inicial do Telegram Bot
  bot.sendMessage(CHAT_ID, "Bot iniciou com sucesso!", "");
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

  digitalWrite(BUZZER_PIN, alertaQueda ? HIGH : LOW);

  // Atualiza OLED a cada 200 ms
  if (millis() - ultimoDisplay >= 200) {
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
