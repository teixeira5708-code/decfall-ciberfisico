#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>
#include <LittleFS.h>

// =====================================================
// WIFI
// =====================================================

const char* NOME_WIFI = "";
const char* SENHA_WIFI = "";

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
bool buzzerLigado = false;

unsigned long tempoQueda = 0;
unsigned long ultimoDisplay = 0;
unsigned long testeBuzzerAte = 0;

// Limites iniciais para teste.
// Considerando aproximadamente 17340 unidades = 1g.
const float LIMITE_QUEDA = 8670;
const float LIMITE_IMPACTO = 43350;

const char* ARQUIVO_LOGS = "/logs.txt";
const size_t TAMANHO_MAX_LOGS = 6000;


// =====================================================
// LOGS (gravados no LittleFS)
// =====================================================

void registrarLog(const String& mensagem) {

  String linha = "[t+" + String(millis() / 1000) + "s] " + mensagem;
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
    valor = Wire.read() << 8;
    valor |= Wire.read();
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
// DETECCAO DE QUEDA
// =====================================================

void detectarQueda() {

  // Calcula a intensidade da aceleracao
  aceleracao = sqrt(
    (float)ax * ax +
    (float)ay * ay +
    (float)az * az
  );

  // Detecta um periodo de queda livre
  if (aceleracao < LIMITE_QUEDA && !alertaQueda && !quedaLivre) {

    quedaLivre = true;

    tempoQueda = millis();
  }

  // Procura um impacto depois da queda livre
  if (quedaLivre && !alertaQueda) {

    if (millis() - tempoQueda <= 1500) {

      if (aceleracao > LIMITE_IMPACTO) {

        alertaQueda = true;

        registrarLog("ALERTA: possivel queda detectada");

      }

    } else {

      quedaLivre = false;

    }
  }
}


// =====================================================
// BOTAO DE RESET
// =====================================================

void verificarBotao() {

  static int ultimaLeitura = HIGH;
  static int estadoEstavel = HIGH;
  static unsigned long ultimoDebounce = 0;

  int leitura = digitalRead(BOTAO_PIN);

  if (leitura != ultimaLeitura) {

    ultimoDebounce = millis();

  }

  if ((millis() - ultimoDebounce) > 50) {

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
// ARQUIVOS DO LITTLEFS
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


// =====================================================
// ENVIAR DADOS PARA A PAGINA
// =====================================================

void enviarDados() {

  String status;

  if (alertaQueda) {
    status = "ALERTA: POSSÍVEL QUEDA DETECTADA!";
  } else {
    status = "Monitorando";
  }

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
}


// =====================================================
// COMANDOS E LOGS
// =====================================================

void tratarComando() {

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
}

void enviarLogs() {

  File f = LittleFS.open(ARQUIVO_LOGS, "r");

  if (!f) {
    server.send(200, "text/plain; charset=utf-8", "Nenhum registro ainda.");
    return;
  }

  server.streamFile(f, "text/plain; charset=utf-8");
  f.close();
}


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  pinMode(BOTAO_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);

  // LittleFS
  if (!LittleFS.begin(true)) {

    Serial.println("Erro ao iniciar LittleFS!");

    while (true) {
      delay(100);
    }

  }

  Serial.println("LittleFS iniciado!");

  // I2C
  Wire.begin(SDA_PIN, SCL_PIN);

  // MPU
  Wire.beginTransmission(MPU_ADDR);

  mpuEncontrado = (Wire.endTransmission() == 0);

  if (mpuEncontrado) {

    Serial.println("MPU encontrado!");

    iniciarMPU();

  } else {

    Serial.println("MPU nao encontrado!");

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

  // Wi-Fi
  WiFi.begin(NOME_WIFI, SENHA_WIFI);

  Serial.print("Conectando ao Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");

  }

  Serial.println();
  Serial.println("Wi-Fi conectado!");

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Rotas dos arquivos (LittleFS)
  server.on("/", HTTP_GET, []() { enviarArquivo("/index.html", "text/html"); });
  server.on("/logs", HTTP_GET, []() { enviarArquivo("/logs.html", "text/html"); });
  server.on("/style.css", HTTP_GET, []() { enviarArquivo("/style.css", "text/css"); });
  server.on("/app.js", HTTP_GET, []() { enviarArquivo("/app.js", "application/javascript"); });

  // Rotas da API
  server.on("/dados", HTTP_GET, enviarDados);
  server.on("/comando", HTTP_GET, tratarComando);
  server.on("/api/logs", HTTP_GET, enviarLogs);

  server.onNotFound([]() {
    server.send(404, "text/plain; charset=utf-8", "Pagina nao encontrada");
  });

  server.begin();

  Serial.println("Servidor HTTP iniciado!");

  registrarLog("Sistema iniciado");

}


// =====================================================
// LOOP
// =====================================================

void loop() {

  // Leitura do acelerometro
  if (mpuEncontrado) {

    ax = ler16(0x3B);
    ay = ler16(0x3D);
    az = ler16(0x3F);

    // Detecta possivel queda
    detectarQueda();

  }

  // Verifica botao
  verificarBotao();

  // Buzzer (alerta ou teste pelo painel)
  buzzerLigado = alertaQueda || (millis() < testeBuzzerAte);
  digitalWrite(BUZZER_PIN, buzzerLigado ? HIGH : LOW);

  // Atualiza OLED a cada 200ms
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

  // Atende o navegador
  server.handleClient();

  delay(20);

}
