#include <Wire.h>
#include <ESP8266WiFi.h>        
#include <ESP8266WebServer.h>   
#include <DHT.h>
#include <Adafruit_BMP280.h>
#include <ThingSpeak.h>

// ===== CONFIGURAR ESTO =====
const char* ssid = "";        // <--- Cambiar esto x el nombre de la red
const char* password = "";    // <--- Cambiar esto x la clave de la red

unsigned long channelID = 3092103;
const char* writeAPIKey = "I6R0FFDE8N84HS78";

// ===== OBJETOS =====
ESP8266WebServer server(80);

#define DHTPIN 2      // Pin donde conectaste el DHT22
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

Adafruit_BMP280 bmp;  // Protocolo I2C

WiFiClient client;

void handleRoot();
void handleData();

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(100);

  dht.begin(); // Inicializacion del sensor DHT11
  
  if (!bmp.begin(0x76)) { // Inicializacion del sensor BMP280
    Serial.println("Error al iniciar BMP280");
    while (1);
  }

  // Conectando al WiFi
  Serial.println("Conectando a WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi conectado!");
  Serial.print("IP local: ");
  Serial.println(WiFi.localIP());

  ThingSpeak.begin(client);

  // Rutas
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
  Serial.println("Servidor web iniciado");
}

void loop() {
  server.handleClient();

  // Leer sensores
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  float p = bmp.readPressure() / 100.0F;

  // Verificar lectura
  if (!isnan(t) && !isnan(h)) {
    ThingSpeak.setField(1, t);
    ThingSpeak.setField(2, h);
    ThingSpeak.setField(3, p);

    int response = ThingSpeak.writeFields(channelID, writeAPIKey);
    if (response == 200) {
      Serial.println("Datos enviados a ThingSpeak!");
    } else {
      Serial.print("Error al enviar a ThingSpeak. Código: ");
      Serial.println(response);
    }
  }

  delay(20000);
}

// ===== HTML principal con Chart.js =====
void handleRoot() {
  String page = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <meta charset="UTF-8">
      <title>Estación Meteorológica</title>
      <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
      <style>
        body {
          background-color: black;
          color: white;
          font-family: Arial, sans-serif;
          text-align: center;
        }
        h1 {
          margin-top: 20px;
        }
        .sensor-container {
          margin: 30px auto;
          width: 80%;
          max-width: 600px;
        }
        canvas {
          background-color: #222;
          border-radius: 10px;
          padding: 10px;
        }
        .value {
          font-size: 1.5em;
          margin: 10px;
        }
      </style>
    </head>
    <body>
      <h1>🌍 Estación Meteorológica</h1>

      <div class="sensor-container">
        <div class="value" id="tempValue">🌡️ Temperatura: -- °C</div>
        <canvas id="tempChart"></canvas>
      </div>

      <div class="sensor-container">
        <div class="value" id="humValue">💧 Humedad: -- %</div>
        <canvas id="humChart"></canvas>
      </div>

      <div class="sensor-container">
        <div class="value" id="presValue">🌪️ Presión: -- hPa</div>
        <canvas id="presChart"></canvas>
      </div>

      <script>
        // ====== Crear gráficos ======
        function createChart(ctx, label, color) {
          return new Chart(ctx, {
            type: 'line',
            data: {
              labels: [],
              datasets: [{ label: label, data: [], borderColor: color, fill: false }]
            },
            options: {
              responsive: true,
              plugins: { legend: { labels: { color: 'white' } } },
              scales: {
                x: { ticks: { color: 'white' } },
                y: { ticks: { color: 'white' } }
              }
            }
          });
        }

        const tempChart = createChart(document.getElementById('tempChart').getContext('2d'), "Temperatura (°C)", "red");
        const humChart  = createChart(document.getElementById('humChart').getContext('2d'), "Humedad (%)", "blue");
        const presChart = createChart(document.getElementById('presChart').getContext('2d'), "Presión (hPa)", "green");

        // ====== Actualizar datos ======
        async function fetchData() {
          const res = await fetch('/data');
          const json = await res.json();
          const time = new Date().toLocaleTimeString();

          // Mostrar valores
          document.getElementById("tempValue").innerText = "🌡️ Temperatura: " + json.temp + " °C";
          document.getElementById("humValue").innerText  = "💧 Humedad: " + json.hum + " %";
          document.getElementById("presValue").innerText = "🌪️ Presión: " + json.pres + " hPa";

          // Agregar a los gráficos
          function updateChart(chart, value) {
            chart.data.labels.push(time);
            chart.data.datasets[0].data.push(value);
            if (chart.data.labels.length > 15) { // mantener últimas 15 muestras
              chart.data.labels.shift();
              chart.data.datasets[0].data.shift();
            }
            chart.update();
          }

          updateChart(tempChart, json.temp);
          updateChart(humChart, json.hum);
          updateChart(presChart, json.pres);
        }

        setInterval(fetchData, 5000); // cada 5 segundos
        fetchData(); // primera llamada inmediata
      </script>
    </body>
    </html>
  )rawliteral";
  server.send(200, "text/html", page);
}



// ===== API JSON con datos de sensores =====
void handleData() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  float p = bmp.readPressure() / 100.0F; // hPa

  if (isnan(t) || isnan(h)) {
    Serial.println("Error al leer DHT11");
    server.send(500, "application/json", "{\"error\":\"Lectura DHT11 fallida\"}");
    return;
  }

  String json = "{";
  json += "\"temp\":" + String(t, 1) + ",";
  json += "\"hum\":" + String(h, 1) + ",";
  json += "\"pres\":" + String(p, 1);
  json += "}";

  server.send(200, "application/json", json);
}
