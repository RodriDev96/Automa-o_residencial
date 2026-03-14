#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <ArduinoOTA.h>

// ===== LOG =====

#define LOGI(fmt, ...) Serial.printf("[INFO ] " fmt "\n", ##__VA_ARGS__)
#define LOGW(fmt, ...) Serial.printf("[WARN ] " fmt "\n", ##__VA_ARGS__)

// ===== PINOS =====

const int BTN[3] = {26,33,16};
const int RELAY[3] = {19,18,17};

const int LED_R=14;
const int LED_G=12;
const int LED_B=27;

// ===== MQTT =====

const char* MQTT_SERVER="192.168.0.107";
const int MQTT_PORT=1883;

const char* TOPIC_SET[3]={
"casa/lampada1/set",
"casa/lampada2/set",
"casa/lampada3/set"
};

const char* TOPIC_STATUS[3]={
"casa/lampada1/status",
"casa/lampada2/status",
"casa/lampada3/status"
};

// ===== OBJETOS =====

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
Preferences prefs;

QueueHandle_t mqttQueue;

// ===== ESTADOS =====

bool lampState[3]={false,false,false};
int btnLast[3]={HIGH,HIGH,HIGH};

volatile bool mqttMsg=false;

// ===== STRUCT =====

struct MQTTMsg
{
uint8_t lamp;
bool state;
};

// ===== LED =====

void setRGB(bool r,bool g,bool b)
{
digitalWrite(LED_R,r);
digitalWrite(LED_G,g);
digitalWrite(LED_B,b);
}

// ===== NVS =====

void saveState(int i)
{
char key[8];
sprintf(key,"lamp%d",i);
prefs.putBool(key,lampState[i]);
}

void loadState()
{
for(int i=0;i<3;i++)
{
char key[8];
sprintf(key,"lamp%d",i);

lampState[i]=prefs.getBool(key,false);

digitalWrite(RELAY[i],lampState[i]);
}
}

// ===== MQTT =====

void publishStatus(int i)
{
mqtt.publish(
TOPIC_STATUS[i],
lampState[i]?"ON":"OFF",
true);
}

void mqttCallback(char* topic, byte* payload, unsigned int length)
{
char msg[32];

if(length>=sizeof(msg))
length=sizeof(msg)-1;

memcpy(msg,payload,length);
msg[length]='\0';

LOGI("MQTT RX %s -> %s",topic,msg);

for(int i=0;i<3;i++)
{
if(strcmp(topic,TOPIC_SET[i])==0)
{
MQTTMsg m;

m.lamp=i;
m.state=(strcmp(msg,"ON")==0);

xQueueSend(mqttQueue,&m,0);

mqttMsg=true;
}
}
}

// ===== TASK PROCESS MQTT =====

void taskMQTTProcess(void *pv)
{
MQTTMsg msg;

for(;;)
{
if(xQueueReceive(mqttQueue,&msg,portMAX_DELAY))
{
lampState[msg.lamp]=msg.state;

digitalWrite(RELAY[msg.lamp],lampState[msg.lamp]);

publishStatus(msg.lamp);

saveState(msg.lamp);
}
}
}

// ===== TASK BOTÕES =====

void taskButtons(void *pv)
{
for(;;)
{
for(int i=0;i<3;i++)
{
int r=digitalRead(BTN[i]);

if(r!=btnLast[i])
{
vTaskDelay(pdMS_TO_TICKS(40));

if(digitalRead(BTN[i])==LOW)
{
lampState[i]=!lampState[i];

digitalWrite(RELAY[i],lampState[i]);

publishStatus(i);

saveState(i);
}

btnLast[i]=r;
}
}

vTaskDelay(pdMS_TO_TICKS(50));
}
}

// ===== TASK MQTT LOOP =====

void taskMQTT(void *pv)
{
for(;;)
{
if(mqtt.connected())
mqtt.loop();

vTaskDelay(pdMS_TO_TICKS(10));
}
}

// ===== TASK RECONNECT =====

void taskReconnect(void *pv)
{
for(;;)
{
if(WiFi.status()!=WL_CONNECTED)
{
WiFi.reconnect();
}

if(!mqtt.connected())
{
LOGW("MQTT reconnect");

String id="ESP32-"+WiFi.macAddress();

if(mqtt.connect(id.c_str()))
{
for(int i=0;i<3;i++)
{
mqtt.subscribe(TOPIC_SET[i]);
publishStatus(i);
}

LOGI("MQTT conectado");
}
}

vTaskDelay(pdMS_TO_TICKS(5000));
}
}

// ===== STATUS =====

void taskStatus(void *pv)
{
unsigned long last=0;

for(;;)
{
if(mqttMsg)
{
setRGB(1,0,1);
vTaskDelay(pdMS_TO_TICKS(200));
mqttMsg=false;
}

if(WiFi.status()!=WL_CONNECTED)
setRGB(1,0,0);

else if(!mqtt.connected())
setRGB(0,0,1);

else
setRGB(0,1,0);

if(millis()-last>10000)
{
LOGI("----- STATUS -----");
LOGI("RSSI %d",WiFi.RSSI());
LOGI("MQTT %s",mqtt.connected()?"OK":"OFF");
LOGI("Heap livre %u",ESP.getFreeHeap());
last=millis();
}

vTaskDelay(pdMS_TO_TICKS(400));
}
}

// ===== OTA =====

void setupOTA()
{
ArduinoOTA.setHostname("esp-lampadas");
ArduinoOTA.setPassword("12345678");

ArduinoOTA.onStart([](){
LOGI("OTA start");
});

ArduinoOTA.onEnd([](){
LOGI("OTA end");
});

ArduinoOTA.begin();
}
void configModeCallback (WiFiManager *myWiFiManager)
{
  Serial.println("Modo configuracao WiFi");

  // LED amarelo (vermelho + verde)
  digitalWrite(LED_R, HIGH);
  digitalWrite(LED_G, HIGH);
  digitalWrite(LED_B, LOW);
}

// ===== SETUP =====

void setup()
{
Serial.begin(115200);

prefs.begin("lampadas",false);

for(int i=0;i<3;i++)
{
pinMode(BTN[i],INPUT_PULLUP);
pinMode(RELAY[i],OUTPUT);
}

pinMode(LED_R,OUTPUT);
pinMode(LED_G,OUTPUT);
pinMode(LED_B,OUTPUT);

mqttQueue=xQueueCreate(10,sizeof(MQTTMsg));

WiFiManager wm;
wm.setAPCallback(configModeCallback);
if(!wm.autoConnect("Configura-Lampadas")){
ESP.restart();
}
setupOTA();

mqtt.setServer(MQTT_SERVER,MQTT_PORT);
mqtt.setCallback(mqttCallback);
mqtt.setBufferSize(512);
mqtt.setSocketTimeout(5);

loadState();

xTaskCreatePinnedToCore(taskButtons,"BTN",4096,NULL,1,NULL,1);
xTaskCreatePinnedToCore(taskMQTTProcess,"MQTTProc",4096,NULL,1,NULL,1);
xTaskCreatePinnedToCore(taskMQTT,"MQTTLoop",4096,NULL,1,NULL,0);
xTaskCreatePinnedToCore(taskReconnect,"MQTTRec",4096,NULL,1,NULL,0);
xTaskCreatePinnedToCore(taskStatus,"Status",4096,NULL,1,NULL,1);

LOGI("Sistema iniciado");
}

// ===== LOOP =====

void loop()
{
ArduinoOTA.handle();
delay(1);
}