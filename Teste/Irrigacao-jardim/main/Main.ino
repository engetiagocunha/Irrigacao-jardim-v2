////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <DHT.h>
#include <Ticker.h>
#include "src/SetupGPIOs.hpp"
#include "src/SetupWiFi.hpp"
#include "src/Sensors.hpp"

// Configurações MQTT
//const char* mqtt_server = "192.168.15.129";
//const int mqtt_port = 1883;
//const char* mqtt_user = "mqtt-user";
//const char* mqtt_password = "1A2b3c4d";

const char* deviceCommandTopic = "/HaOS/esp01/device1/set";
const char* deviceStateTopic = "/HaOS/esp01/device1/state";
const char* temperatureTopic = "/HaOS/esp01/sensor/temperature";
const char* humidityTopic = "/HaOS/esp01/sensor/humidity";
const char* soilMoistureTopic = "/HaOS/esp01/sensor/soilMoisture";

// Protótipos das funções
void setupMQTT();
void mqttLoop();
void controlRelay(bool state);

WiFiClient espClient;
PubSubClient mqttClient(espClient);
Preferences preferences;
Ticker sensorTicker;

bool manualOverride = false;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Controle do relé
void controlRelay(bool state) {
  digitalWrite(RELAY_PIN, state ? LOW : HIGH);              // LOW liga, HIGH desliga
  preferences.putBool("relay", state);                      // Salva estado no armazenamento
  mqttClient.publish(deviceStateTopic, state ? "1" : "0");  // Publica estado no MQTT
  Serial.println(state ? "Relé ligado" : "Relé desligado");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// função de configuração principal
void setup() {
  Serial.begin(115200);
  setupGPIOs();
  setupSensors();
  setupWiFi();

  // Recupera o estado salvo do relé (Se for a primeira vez o relé inicia deslidado "true" - lógica reversa)
  preferences.begin("my-app", false);
  bool relayState = preferences.getBool("relay", true);
  controlRelay(relayState);

  // Configuração mqtt
  setupMQTT();

  // Atualiza dados dos sensores periodicamente
  sensorTicker.attach(5, []() {
    readDHT();
    readSoilMoisture();
    mqttLoop();
  });
}

// Função Loop principal
void loop() {
  if (!mqttClient.connected()) {
    setupMQTT();
  }
  mqttClient.loop();
  checkButtonReset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Configura e gerencia a conexão MQTT
void setupMQTT() {
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback([](char* topic, byte* payload, unsigned int length) {
    String message;
    for (unsigned int i = 0; i < length; i++) {
      message += (char)payload[i];
    }

    if (String(topic) == deviceCommandTopic) {
      bool newState = (message == "0");  // "0" liga o relé, "1" desliga
      manualOverride = true;
      controlRelay(newState);
    }
  });

  if (!mqttClient.connected()) {
    Serial.print("Conectando ao MQTT...");
    if (mqttClient.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("Conectado");
      mqttClient.subscribe(deviceCommandTopic);
      mqttClient.publish(deviceStateTopic, digitalRead(RELAY_PIN) == LOW ? "1" : "0");
    } else {
      Serial.print("Falha na conexão MQTT, rc=");
      Serial.println(mqttClient.state());
    }
  }
}


// Gerencia a lógica de envio dos dados via MQTT
void mqttLoop() {
  // Envia dados do sensor via MQTT
  mqttClient.publish(temperatureTopic, String(temperatura).c_str());
  mqttClient.publish(humidityTopic, String(umidade).c_str());
  mqttClient.publish(soilMoistureTopic, String(soilmoisture).c_str());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////