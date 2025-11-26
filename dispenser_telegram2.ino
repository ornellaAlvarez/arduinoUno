#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Servo.h>
#include <time.h>

// ======================= CONFIG ============================
const char* ssid = "iPhone de Ornella";
const char* password = "Ornalla0";
#define BOT_TOKEN "7947545042:AAFNE-uWWvxOTiDC5CJOrQap-2GeM3H2UN0"
#define CHAT_ID "7145471122"

#define TRIG_PIN D5
#define ECHO_PIN D6
#define SERVO_PIN D7

Servo servo;
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// ======================= VARIABLES ============================
int intervalo = 10;
unsigned long ultimoTiempo = 0;

int anguloApertura = 180;
bool notificaciones = true;
int umbralVacio = 10;

unsigned long ultimoServo = 0;
bool servoAbriendo = false;

// HORARIOS
String horarios[10];
int totalHorarios = 0;
String ultimoHorarioEjecutado = "";

// ESTADOS DEL MENÚ
enum EstadoMenu {
  MENU_PRINCIPAL,
  ESPERANDO_INTERVALO,
  ESPERANDO_ANGULO,
  ESPERANDO_UMBRAL,
  ESPERANDO_HORARIO_NUEVO,
  ESPERANDO_HORARIO_BORRAR
};

EstadoMenu estadoActual = MENU_PRINCIPAL;

// ======================= FUNCIONES ============================

// ---- Medición del nivel ----
float medirNivel() {
  float suma = 0;
  for (int i = 0; i < 5; i++) {
    digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duracion = pulseIn(ECHO_PIN, HIGH, 30000);
    suma += duracion * 0.034 / 2;
    delay(20);
  }

  float distancia = suma / 5;
  float nivel = map(distancia, 20, 5, 0, 100);
  return constrain(nivel, 0, 100);
}

// ---- Servo sin delay ----
void abrirServo() {
  servo.write(anguloApertura);
  servoAbriendo = true;
  ultimoServo = millis();
}

void actualizarServo() {
  if (servoAbriendo && millis() - ultimoServo >= 2500) {
    servo.write(0);
    servoAbriendo = false;
  }
}

// ---- Obtener hora desde NTP ----
String obtenerHora() {
  time_t now = time(nullptr);
  struct tm* p = localtime(&now);

  char buffer[6];
  sprintf(buffer, "%02d:%02d", p->tm_hour, p->tm_min);

  return String(buffer);
}

// ---- Verificar horarios programados ----
void revisarHorarios() {
  String horaActual = obtenerHora();

  if (horaActual == ultimoHorarioEjecutado) return;

  for (int i = 0; i < totalHorarios; i++) {
    if (horarios[i] == horaActual) {
      abrirServo();
      ultimoHorarioEjecutado = horaActual;

      bot.sendMessage(CHAT_ID, "⏰ Alimentado automáticamente a las " + horaActual, "");
    }
  }
}

// ---- Menú principal ----
void menuPrincipal(String chat_id) {
  String kb =
    "[[ {\"text\":\"🍽 Servir\",\"callback_data\":\"servir\"} ],"
    " [ {\"text\":\"📊 Nivel\",\"callback_data\":\"nivel\"},"
    "   {\"text\":\"⏱ Intervalo\",\"callback_data\":\"intervalo\"} ],"
    " [ {\"text\":\"⏰ Horarios\",\"callback_data\":\"horarios\"} ],"
    " [ {\"text\":\"🔔 Notif\",\"callback_data\":\"notif\"},"
    "   {\"text\":\"📝 Estado\",\"callback_data\":\"estado\"} ],"
    " [ {\"text\":\"⚙ Avanzado\",\"callback_data\":\"config\"} ]]";

  bot.sendMessageWithInlineKeyboard(chat_id,
    "📍 *MENÚ PRINCIPAL*",
    "Markdown", kb);
}

// ---- Submenú de horarios ----
void menuHorarios(String chat_id) {
  String kb =
    "[[ {\"text\":\"➕ Agregar\",\"callback_data\":\"addhora\"} ],"
    " [ {\"text\":\"📋 Ver\",\"callback_data\":\"verhora\"} ],"
    " [ {\"text\":\"🗑 Borrar\",\"callback_data\":\"delhora\"} ],"
    " [ {\"text\":\"🔙 Volver\",\"callback_data\":\"volver\"} ]]";

  bot.sendMessageWithInlineKeyboard(chat_id,
    "⏰ *Gestión de horarios*",
    "Markdown", kb);
}

// ---- Submenú avanzado ----
void menuConfig(String chat_id) {
  String kb =
    "[[ {\"text\":\"🪝 Ángulo\",\"callback_data\":\"angulo\"} ],"
    " [ {\"text\":\"🚨 Umbral vacío\",\"callback_data\":\"umbral\"} ],"
    " [ {\"text\":\"🔙 Volver\",\"callback_data\":\"volver\"} ]]";

  bot.sendMessageWithInlineKeyboard(chat_id,
    "*Configuración avanzada* ⚙",
    "Markdown", kb);
}

// ---- Control por intervalo ----
void controlarDispenser() {
  float nivel = medirNivel();

  if (nivel < umbralVacio && notificaciones) {
    bot.sendMessage(CHAT_ID, "⚠ ¡El dispenser está vacío!", "");
    notificaciones = false;
  }
  if (nivel >= umbralVacio) {
    notificaciones = true;
  }

  if (millis() - ultimoTiempo >= intervalo * 60000UL) {
    abrirServo();
    ultimoTiempo = millis();
    if (notificaciones)
      bot.sendMessage(CHAT_ID, "⏱ Alimentación automática por intervalo.", "");
  }
}

// ======================= PROCESAR MENSAJES =======================
void handleNewMessages(int numNewMessages) {

  for (int i = 0; i < numNewMessages; i++) {

    String text = bot.messages[i].text;
    String chat_id = bot.messages[i].chat_id;

    if (chat_id != CHAT_ID) continue;

    // /menu y /start
    if (text == "/menu" || text == "/start") {
      estadoActual = MENU_PRINCIPAL;
      menuPrincipal(chat_id);
      continue;
    }

    // BOTONES INLINE
    if (bot.messages[i].type == "callback_query") {
      String data = bot.messages[i].text;

      // ----- SERVIR -----
      if (data == "servir") {
        abrirServo();
        bot.sendMessage(chat_id, "🍽 ¡Comida servida!", "");
      }

      // ----- NIVEL -----
      else if (data == "nivel") {
        float nivel = medirNivel();
        bot.sendMessage(chat_id, "📊 Nivel actual: " + String(nivel, 1) + "%", "");
      }

      // ----- INTERVALO -----
      else if (data == "intervalo") {
        estadoActual = ESPERANDO_INTERVALO;
        bot.sendMessage(chat_id, "⏱ Ingresa el intervalo (minutos):", "");
      }

      // ----- HORARIOS -----
      else if (data == "horarios") menuHorarios(chat_id);

      else if (data == "addhora") {
        estadoActual = ESPERANDO_HORARIO_NUEVO;
        bot.sendMessage(chat_id, "⏰ Ingresa un horario (HH:MM):", "");
      }

      else if (data == "verhora") {
        if (totalHorarios == 0)
          bot.sendMessage(chat_id, "📋 No hay horarios programados.", "");
        else {
          String lista = "📅 *Horarios programados:*\n";
          for (int i = 0; i < totalHorarios; i++)
            lista += "• " + horarios[i] + "\n";
          bot.sendMessage(chat_id, lista, "Markdown");
        }
      }

      else if (data == "delhora") {
        estadoActual = ESPERANDO_HORARIO_BORRAR;
        bot.sendMessage(chat_id, "🗑 Ingresa el número del horario a borrar:", "");
      }

      // ----- CONFIGURACIÓN AVANZADA -----
      else if (data == "config") menuConfig(chat_id);

      else if (data == "angulo") {
        estadoActual = ESPERANDO_ANGULO;
        bot.sendMessage(chat_id, "🪝 Ingresa el ángulo (50–180):", "");
      }

      else if (data == "umbral") {
        estadoActual = ESPERANDO_UMBRAL;
        bot.sendMessage(chat_id, "🚨 Ingresa el umbral (%):", "");
      }

      else if (data == "notif") {
        notificaciones = !notificaciones;
        bot.sendMessage(chat_id, String("🔔 Notificaciones: ") + (notificaciones ? "ON" : "OFF"), "");
      }

      else if (data == "estado") {
        float nivel = medirNivel();
        bot.sendMessage(
          chat_id,
          "📝 *ESTADO DEL SISTEMA*\n\n"
          "Nivel: " + String(nivel,1) + "%\n"
          "Intervalo: " + String(intervalo) + " min\n"
          "Ángulo: " + String(anguloApertura) + "°\n"
          "Umbral vacío: " + String(umbralVacio) + "%\n"
          "Notificaciones: " + (notificaciones ? "ON" : "OFF") + "\n"
          "Horarios programados: " + String(totalHorarios),
          "Markdown"
        );
      }

      else if (data == "volver") menuPrincipal(chat_id);
    }

    // ======== ENTRADA DE TEXTO (CONFIGURACIONES) =========

    // ----- INTERVALO -----
    if (estadoActual == ESPERANDO_INTERVALO) {
      int val = text.toInt();
      if (val > 0 && val < 600) {
        intervalo = val;
        bot.sendMessage(chat_id, "⏱ Intervalo actualizado.", "");
      }
      estadoActual = MENU_PRINCIPAL;
      menuPrincipal(chat_id);
    }

    // ----- ÁNGULO -----
    else if (estadoActual == ESPERANDO_ANGULO) {
      int val = text.toInt();
      if (val >= 50 && val <= 180) {
        anguloApertura = val;
        bot.sendMessage(chat_id, "🪝 Ángulo actualizado.", "");
      }
      estadoActual = MENU_PRINCIPAL;
      menuPrincipal(chat_id);
    }

    // ----- UMBRAL VACÍO -----
    else if (estadoActual == ESPERANDO_UMBRAL) {
      int val = text.toInt();
      if (val >= 1 && val <= 100) {
        umbralVacio = val;
        bot.sendMessage(chat_id, "🚨 Umbral actualizado.", "");
      }
      estadoActual = MENU_PRINCIPAL;
      menuPrincipal(chat_id);
    }

    // ----- AGREGAR HORARIO -----
    else if (estadoActual == ESPERANDO_HORARIO_NUEVO) {
      if (text.length() == 5 && text[2] == ':' && totalHorarios < 10) {
        horarios[totalHorarios++] = text;
        bot.sendMessage(chat_id, "⏰ Horario agregado.", "");
      } else {
        bot.sendMessage(chat_id, "Formato inválido. Ejemplo correcto: 08:30", "");
      }
      estadoActual = MENU_PRINCIPAL;
      menuPrincipal(chat_id);
    }

    // ----- BORRAR HORARIO -----
    else if (estadoActual == ESPERANDO_HORARIO_BORRAR) {
      int idx = text.toInt() - 1;
      if (idx >= 0 && idx < totalHorarios) {
        for (int j = idx; j < totalHorarios - 1; j++)
          horarios[j] = horarios[j + 1];
        totalHorarios--;
        bot.sendMessage(chat_id, "🗑 Horario eliminado.", "");
      } else {
        bot.sendMessage(chat_id, "Número inválido.", "");
      }
      estadoActual = MENU_PRINCIPAL;
      menuPrincipal(chat_id);
    }
  }
}

// ======================= SETUP Y LOOP ============================
void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  servo.attach(SERVO_PIN);
  servo.write(0);

  client.setInsecure();
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(300);

  // CONFIGURAR NTP
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  bot.sendMessage(CHAT_ID, "🤖 Dispenser conectado!\nUsa /menu", "");
}

void loop() {
  actualizarServo();
  controlarDispenser();
  revisarHorarios();

  int newMsg = bot.getUpdates(bot.last_message_received + 1);
  while (newMsg) {
    handleNewMessages(newMsg);
    newMsg = bot.getUpdates(bot.last_message_received + 1);
  }
}
