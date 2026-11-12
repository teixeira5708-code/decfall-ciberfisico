#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =====================================================
// WIFI
// =====================================================

const char* NOME_WIFI = "SEU_WIFI";
const char* SENHA_WIFI = "SUA_SENHA";

WebServer server(80);


// =====================================================
// I2C
// =====================================================

#define SDA_PIN 21
#define SCL_PIN 22

#define MPU_ADDR 0x68
#define OLED_ADDR 0x3C


// =====================================================
// OLED
// =====================================================

#define LARGURA 128
#define ALTURA 64

Adafruit_SSD1306 display(LARGURA, ALTURA, &Wire, -1);


// =====================================================
// VARIÁVEIS DO ACELERÔMETRO
// =====================================================

int16_t ax = 0;
int16_t ay = 0;
int16_t az = 0;


// =====================================================
// LER 16 BITS DO MPU
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
// PÁGINA WEB
// =====================================================

void paginaInicial() {

  String html = R"rawliteral(

<!DOCTYPE html>

<html>

<head>

<meta charset="UTF-8">

<meta name="viewport" content="width=device-width, initial-scale=1.0">

<title>Detector de Quedas</title>

<style>

body {
  font-family: Arial, sans-serif;
  background: #f2f2f2;
  text-align: center;
  margin: 0;
  padding: 30px;
}

h1 {
  color: #222;
}

.card {
  max-width: 500px;
  margin: auto;
  background: white;
  padding: 25px;
  border-radius: 15px;
  box-shadow: 0 4px 15px rgba(0,0,0,0.15);
}

.valor {
  font-size: 28px;
  margin: 15px;
}

.status {
  margin-top: 20px;
  padding: 15px;
  border-radius: 10px;
  background: #ddd;
}

</style>

</head>


<body>

<div class="card">

<h1>Detector de Quedas</h1>

<hr>

<div class="valor">
X: <span id="x">0</span>
</div>

<div class="valor">
Y: <span id="y">0</span>
</div>

<div class="valor">
Z: <span id="z">0</span>
</div>

<div class="status">
Status: <b id="status">Monitorando</b>
</div>

</div>


<script>

function atualizar() {

  fetch('/dados')

  .then(response => response.json())

  .then(data => {

    document.getElementById('x').innerText = data.x;

    document.getElementById('y').innerText = data.y;

    document.getElementById('z').innerText = data.z;

    document.getElementById('status').innerText = data.status;

  });

}

setInterval(atualizar, 500);

atualizar();

</script>


</body>

</html>

)rawliteral";


  server.send(200, "text/html", html);
}


// =====================================================
// ENVIAR DADOS PARA O SITE
// =====================================================

void enviarDados() {

  String status = "Monitorando";

  String json = "{";

  json += "\"x\":" + String(ax) + ",";

  json += "\"y\":" + String(ay) + ",";

  json += "\"z\":" + String(az) + ",";

  json += "\"status\":\"" + status + "\"";

  json += "}";

  server.send(200, "application/json", json);
}


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);


  // ===================================================
  // INICIA I2C
  // ===================================================

  Wire.begin(SDA_PIN, SCL_PIN);


  // ===================================================
  // TESTA MPU
  // ===================================================

  Wire.beginTransmission(MPU_ADDR);

  if (Wire.endTransmission() == 0) {

    Serial.println("MPU encontrado!");

  } else {

    Serial.println("MPU nao encontrado!");

  }


  // ===================================================
  // INICIA OLED
  // ===================================================

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {

    Serial.println("OLED nao encontrado!");

    while (true) {

      delay(100);

    }

  }

  Serial.println("OLED encontrado!");


  // ===================================================
  // TELA INICIAL
  // ===================================================

  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);

  display.setCursor(0, 0);

  display.println("Detector");

  display.println("de Quedas");

  display.display();

  delay(2000);


  // ===================================================
  // CONECTA NO WIFI
  // ===================================================

  Serial.println();

  Serial.print("Conectando ao WiFi");

  WiFi.begin(NOME_WIFI, SENHA_WIFI);


  while (WiFi.status() != WL_CONNECTED) {

    delay(500);

    Serial.print(".");

  }


  Serial.println();

  Serial.println("WiFi conectado!");

  Serial.print("IP do ESP32: ");

  Serial.println(WiFi.localIP());


  // ===================================================
  // ROTAS DO SERVIDOR
  // ===================================================

  server.on("/", paginaInicial);

  server.on("/dados", enviarDados);


  // ===================================================
  // INICIA SERVIDOR
  // ===================================================

  server.begin();

  Serial.println("Servidor HTTP iniciado!");

}


// =====================================================
// LOOP
// =====================================================

void loop() {

  // Lê acelerômetro

  ax = ler16(0x3B);

  ay = ler16(0x3D);

  az = ler16(0x3F);


  // ===================================================
  // MONITOR SERIAL
  // ===================================================

  Serial.print("X: ");

  Serial.print(ax);

  Serial.print(" | Y: ");

  Serial.print(ay);

  Serial.print(" | Z: ");

  Serial.println(az);


  // ===================================================
  // OLED
  // ===================================================

  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);

  display.setCursor(0, 0);

  display.println("ACELEROMETRO");


  display.setCursor(0, 18);

  display.print("X: ");

  display.println(ax);


  display.setCursor(0, 32);

  display.print("Y: ");

  display.println(ay);


  display.setCursor(0, 46);

  display.print("Z: ");

  display.println(az);


  display.display();


  // ===================================================
  // SERVIDOR
  // ===================================================

  server.handleClient();

  delay(100);

}