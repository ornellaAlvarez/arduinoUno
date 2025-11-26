#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Servo.h>
#include <time.h>

// ------------ CONFIGURACIÓN ------------
const char* ssid = "TU_WIFI";
const char* password = "TU_CONTRASEÑA";
#define BOT_TOKEN "7947545042:AAFNE-uWWvxOTiDC5CJOrQap-2GeM3H2UN0"
#define CHAT_ID "7145471122"

#define TRIG_PIN D5
#define ECHO_PIN D6
#define SERVO_PIN D7

// Tiempo NTP
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -3 * 3600;   // UTC -3 (Argentina)
const int daylightOffset_sec = 0;

// Historial (máximo 10 registros)
String historial[10];
int totalRegistros = 0;

// ------------ VARIABLES ------------
Servo servo;
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

int intervalo = 10;  // minutos
unsigned long ultimoTiempo = 0;
bool dispenserVacio = false;

// ------------ FUNCIONES ------------

// Obtener fecha y hora reales
String obtenerFechaHora() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "ERROR HORA";
  }

  char buffer[30];
  strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
  return String(buffer);
}

// Guardar un registro en el historial
void guardarEnHistorial(String evento) {
  if (totalRegistros >= 10) {
    for (int i = 9; i > 0; i--) {
      historial[i] = historial[i - 1];
    }
    historial[0] = evento;
  } else {
    for (int i = totalRegistros; i > 0; i--) {
      historial[i] = historial[i - 1];
    }
    historial[0] = evento;
    totalRegistros++;
  }
}

// Medir nivel con ultrasonido
float medirNivel() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duracion = pulseIn(ECHO_PIN, HIGH);
  float distancia = duracion * 0.034 / 2;

  float nivel = map(distancia, 20, 5, 0, 100);
  nivel = constrain(nivel, 0, 100);
  return nivel;
}

// Abrir servo y registrar evento
void abrirServo() {
  servo.write(90);
  delay(2000);
  servo.write(0);

  String fecha = obtenerFechaHora();
  guardarEnHistorial("Comida servida el " + fecha);
}

// Control automático
void controlarDispenser() {
  float nivel = medirNivel();

  if (nivel < 10 && !dispenserVacio) {
    bot.sendMessage(CHAT_ID, "¡El dispenser está vacío!", "");
    dispenserVacio = true;
  } else if (nivel >= 10) {
    dispenserVacio = false;
  }

  unsigned long ahora = millis();
  if (ahora - ultimoTiempo >= intervalo * 60000) {
    abrirServo();

    String fecha = obtenerFechaHora();
    bot.sendMessage(CHAT_ID, "Alimentación automática realizada el " + fecha, "");

    ultimoTiempo = ahora;
  }
}

// Procesar comandos de Telegram
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String text = bot.messages[i].text;
    String chat_id = bot.messages[i].chat_id;

    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "No autorizado.", "");
      continue;
    }

    if (text == "/start") {
      bot.sendMessage(chat_id,
        "Bienvenido al Dispenser Automático!\n"
        "Comandos disponibles:\n"
        "• /abrir - Liberar comida\n"
        "• /nivel - Ver nivel de comida\n"
        "• /intervalo X - Ajustar intervalo (min)\n"
        "• /historial - Ver últimas alimentaciones\n",
        "");
    }

    else if (text == "/abrir") {
      abrirServo();
      bot.sendMessage(chat_id, "¡Comida servida!", "");
    }

    else if (text.startsWith("/intervalo")) {
      int nuevo = text.substring(11).toInt();
      if (nuevo > 0) {
        intervalo = nuevo;
        bot.sendMessage(chat_id, "Intervalo ajustado a " + String(intervalo) + " min.", "");
      } else {
        bot.sendMessage(chat_id, "Usa /intervalo X (ej: /intervalo 15)", "");
      }
    }

    else if (text == "/nivel") {
      float nivel = medirNivel();
      bot.sendMessage(chat_id, "Nivel de comida: " + String(nivel, 1) + "%", "");
    }

    else if (text == "/historial") {
      if (totalRegistros == 0) {
        bot.sendMessage(chat_id, "No hay registros todavía.", "");
      } else {
        String mensaje = "*Últimos registros de alimentación:*\n\n";
        for (int j = 0; j < totalRegistros; j++) {
          mensaje += String(j + 1) + ". " + historial[j] + "\n";
        }
        bot.sendMessage(chat_id, mensaje, "Markdown");
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  servo.attach(SERVO_PIN);
  servo.write(0);

  client.setInsecure();

  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectada");
  Serial.println(WiFi.localIP());

  // Sincronizar hora
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  delay(2000);

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    Serial.println("Hora NTP sincronizada.");
  } else {
    Serial.println("ERROR obteniendo hora NTP.");
  }

  bot.sendMessage(CHAT_ID, "Dispenser conectado y listo!", "");
}

void loop() {
  controlarDispenser();

  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  while (numNewMessages) {
    handleNewMessages(numNewMessages);
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }

  delay(1000);
}
