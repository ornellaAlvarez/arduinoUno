#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Servo.h>

// CONFIGURACIÓN
const char* ssid = "TU_WIFI";
const char* password = "TU_CONTRASEÑA";
#define BOT_TOKEN "TU_TELEGRAM_BOT_TOKEN"
#define CHAT_ID "TU_CHAT_ID"

#define TRIG_PIN D5
#define ECHO_PIN D6
#define SERVO_PIN D7

Servo servo;
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

int intervalo = 10;  // minutos
unsigned long ultimoTiempo = 0;
bool dispenserVacio = false;

// Mide nivel de comida (ultrasonido)
float medirNivel() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duracion = pulseIn(ECHO_PIN, HIGH);
  float distancia = duracion * 0.034 / 2;
  
  // 5 cm lleno, 20 cm vacío
  float nivel = map(distancia, 20, 5, 0, 100);
  nivel = constrain(nivel, 0, 100);
  return nivel;
}

// Abre el compartimento
void abrirServo() {
  servo.write(90);
  delay(2000);
  servo.write(0);
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
    ultimoTiempo = ahora;
  }
}

// Procesar comandos de Telegram
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String text = bot.messages[i].text;
    String chat_id = bot.messages[i].chat_id;
    Serial.println("Mensaje: " + text);

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
        "• /intervalo X - Ajustar intervalo (minutos)",
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
        bot.sendMessage(chat_id, "Usa /intervalo X (ejemplo: /intervalo 15)", "");
      }
    }
    else if (text == "/nivel") {
      float nivel = medirNivel();
      bot.sendMessage(chat_id, "Nivel de comida: " + String(nivel, 1) + "%", "");
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  servo.attach(SERVO_PIN);
  servo.write(0);

  client.setInsecure();  // para conexión TLS
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectada");
  Serial.println(WiFi.localIP());

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
