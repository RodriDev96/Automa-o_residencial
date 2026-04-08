#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <ArduinoOTA.h>

// =========================
// LOG
// =========================
#define LOG(msg) Serial.println(msg)
#define LOGF(...) Serial.printf(__VA_ARGS__)

// =========================
// VARIÁVEIS
// =========================
bool novaMensagem = false;

WiFiClient espClient;
PubSubClient client(espClient);
Preferences prefs;

// Botões
const int botoes[3] = { 26, 33, 16 };

// Relés
const int saidas[3] = { 19, 18, 17 };

// LED RGB
const int LED_R = 14;
const int LED_G = 12;
const int LED_B = 27;

bool estadoLamps[3] = { false, false, false };
int estadoAnterior[3] = { HIGH, HIGH, HIGH };
unsigned long lastDebounce[3] = { 0 };

// MQTT
const int mqtt_port = 1883;
const char* mqtt_server = "raspberrypi.local";

const char* topicos_set[3] = {
  "casa/lampada1/set",
  "casa/lampada2/set",
  "casa/lampada3/set"
};

const char* topicos_status[3] = {
  "casa/lampada1/status",
  "casa/lampada2/status",
  "casa/lampada3/status"
};

// =========================
// LED RGB
// =========================
void setRGB(bool r, bool g, bool b) {
  digitalWrite(LED_R, r);
  digitalWrite(LED_G, g);
  digitalWrite(LED_B, b);
}

// =========================
// EEPROM
// =========================
void salvarEstado(int i) {
  prefs.putBool(("lamp" + String(i)).c_str(), estadoLamps[i]);
  LOGF("[EEPROM] Lamp %d salva = %s\n", i + 1, estadoLamps[i] ? "ON" : "OFF");
}

void carregarEstados() {
  for (int i = 0; i < 3; i++) {
    estadoLamps[i] = prefs.getBool(("lamp" + String(i)).c_str(), false);
    digitalWrite(saidas[i], estadoLamps[i] ? HIGH : LOW);

    LOGF("[BOOT] Lamp %d = %s\n", i + 1, estadoLamps[i] ? "ON" : "OFF");
  }
}

// =========================
// MQTT CALLBACK
// =========================
void callback(char* topic, byte* payload, unsigned int length) {
  novaMensagem = true;

  String msg;
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  LOGF("[MQTT] %s -> %s\n", topic, msg.c_str());

  for (int i = 0; i < 3; i++) {
    if (String(topic) == topicos_set[i]) {
      estadoLamps[i] = (msg == "ON");

      digitalWrite(saidas[i], estadoLamps[i]);

      if (client.connected()) {
        client.publish(topicos_status[i],
                       estadoLamps[i] ? "ON" : "OFF", true);
      }

      salvarEstado(i);
    }
  }
}

// =========================
// MQTT RECONNECT
// =========================
void reconnectTask(void* parameter) {
  String clientId = "ESP32_" + String((uint32_t)ESP.getEfuseMac(), HEX);

  while (true) {
    if (!client.connected()) {

      LOG("[MQTT] Tentando conectar...");

      if (client.connect(clientId.c_str())) {

        LOG("[MQTT] Conectado!");

        client.publish("casa/status", "ONLINE", true);

        for (int i = 0; i < 3; i++) {
          client.subscribe(topicos_set[i]);

          LOGF("[MQTT] Subscrito: %s\n", topicos_set[i]);

          client.publish(topicos_status[i],
                         estadoLamps[i] ? "ON" : "OFF", true);
        }
      } else {
        LOGF("[MQTT] Falhou rc=%d\n", client.state());
      }
    }

    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

// =========================
// BOTÕES
// =========================
const unsigned long debounceDelay = 80;

void taskBotoes(void *parameter) {
  Serial.printf("Task rodando: %s\n", pcTaskGetName(NULL));

  while (true) {
    for (int i = 0; i < 3; i++) {

      int leitura = digitalRead(botoes[i]);

      if (leitura != estadoAnterior[i]) {

        vTaskDelay(50 / portTICK_PERIOD_MS);

        leitura = digitalRead(botoes[i]);

        if (leitura != estadoAnterior[i]) {

          estadoAnterior[i] = leitura;

          // 🔁 Alterna em QUALQUER mudança
          estadoLamps[i] = !estadoLamps[i];

          digitalWrite(saidas[i], estadoLamps[i]);

          LOGF("[BOTAO] Lamp %d -> %s\n",
               i + 1, estadoLamps[i] ? "ON" : "OFF");

          if (client.connected()) {
            client.publish(topicos_status[i],
                           estadoLamps[i] ? "ON" : "OFF", true);
          }

          salvarEstado(i);
        }
      }
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}
// =========================
// MQTT LOOP
// =========================
void taskMQTTLoop(void* parameter) {
  while (true) {
    if (client.connected()) {
      client.loop();
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// =========================
// OTA TASK
// =========================
void taskOTA(void* parameter) {
  while (true) {
    ArduinoOTA.handle();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// =========================
// LED STATUS
// =========================
void taskStatusLED(void* parameter) {
  while (true) {

    if (novaMensagem) {
      setRGB(1, 0, 1);  // roxo
      vTaskDelay(200 / portTICK_PERIOD_MS);
      novaMensagem = false;
    }

    if (WiFi.status() != WL_CONNECTED) {
      setRGB(1, 0, 0);  // vermelho
    } else if (!client.connected()) {
      setRGB(0, 0, 1);  // azul
    } else {
      setRGB(0, 1, 0);  // verde
    }

    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

// =========================
// SETUP
// =========================
void setup() {
  Serial.begin(115200);

  prefs.begin("lampadas", false);

  for (int i = 0; i < 3; i++) {
    pinMode(botoes[i], INPUT_PULLUP);
    pinMode(saidas[i], OUTPUT);
    digitalWrite(saidas[i], LOW);
    estadoAnterior[i] = digitalRead(botoes[i]);
  }

  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);

  WiFiManager wm;
  wm.setTimeout(180);

  if (!wm.autoConnect("Configura-Lampadas")) {
    ESP.restart();
  }

  LOG("========== WIFI ==========");
  LOGF("IP: %s\n", WiFi.localIP().toString().c_str());
  LOGF("SSID: %s\n", WiFi.SSID().c_str());
  LOG("==========================");

  carregarEstados();

  // MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // OTA
  ArduinoOTA.setHostname("ESP32-Lampadas");

  ArduinoOTA.onStart([]() {
    LOG("[OTA] Iniciando...");
  });

  ArduinoOTA.onEnd([]() {
    LOG("[OTA] Finalizado!");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    LOGF("[OTA] %u%%\n", (progress / (total / 100)));
  });

  ArduinoOTA.begin();

  // TASKS
  xTaskCreatePinnedToCore(taskMQTTLoop, "MQTT", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(reconnectTask, "Reconnect", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(taskBotoes, "Botoes", 2048, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(taskStatusLED, "LED", 2048, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(taskOTA, "OTA", 4096, NULL, 1, NULL, 1);
}

void loop() {
  // FreeRTOS
}
