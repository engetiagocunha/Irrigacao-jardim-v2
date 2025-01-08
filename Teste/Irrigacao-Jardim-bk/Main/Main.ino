#include <WiFi.h>
#include <WiFiClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <DHT.h>
#include <Ticker.h>
#include "src/SetupGPIOs.hpp"
#include "src/SetupWiFi.hpp"
#include "src/Sensors.hpp"
#include "src/PageHTML.hpp"

// Definições de variáveis globais
Ticker sensorTicker;
Preferences preferences;

// Configurações do MQTT
const char* mqtt_server = "192.168.15.129";
const int mqtt_port = 1883;
const char* mqtt_user = "mqtt-user";
const char* mqtt_password = "1A2b3c4d";

const char* deviceCommandTopic = "/HaOS/esp01/device1/set";
const char* deviceStateTopic = "/HaOS/esp01/device1/state";
const char* temperatureTopic = "/HaOS/esp01/sensor/temperature";
const char* humidityTopic = "/HaOS/esp01/sensor/humidity";
const char* soilMoistureTopic = "/HaOS/esp01/sensor/soilMoisture";

WebServer server(80);
WebSocketsServer webSocket(81);
WiFiClient espClient;
PubSubClient mqttClient(espClient);
bool loggedIn = false;
bool manualOverride = false;
const char* defaultUsername = "admin";
const char* defaultPassword = "admin";

// Variáveis de configurações do timer
int hora, minuto, segundo, diaDaSemana;
int configHoraInicio, configMinutoInicio, configHoraFim, configMinutoFim;
bool diasDaSemana[7] = { false, false, false, false, false, false, false };

// Configuração do NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -10800, 60000);

// Protótipos das funções
void updateSensorData();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void checkAlarmAndControlRelay();
void controlDevice(bool state);
void notifyClients();
void connectToMqtt();
void sendSensorDataMQTT();
void setupServer();
void handleRoot();
void handleConfigPost();
void handleLogin();
void handleLoginPost();
void handleLogout();
void handleNotFound();
void updateHTMLPlaceholders(String& html);
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);

// Função de configuração inicial
void setup() {
  Serial.begin(115200);
  setupGPIOs();
  setupSensors();
  setupWiFi();
  setupMDNS();

  preferences.begin("my-app", false);

  // Recupera o estado salvo do relé
  bool relayState = preferences.getBool("relay", false);
  digitalWrite(RELAY_PIN, relayState ? LOW : HIGH);

  // Carrega configurações com valores padrão
  configHoraInicio = preferences.getInt("horaInicio", 0);
  configMinutoInicio = preferences.getInt("minutoInicio", 0);
  configHoraFim = preferences.getInt("horaFim", 23);
  configMinutoFim = preferences.getInt("minutoFim", 59);

  for (int i = 0; i < 7; i++) {
    diasDaSemana[i] = preferences.getBool(("dia" + String(i)).c_str(), false);
  }

  setupServer();
  timeClient.begin();

  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
  connectToMqtt();

  checkAlarmAndControlRelay();
  sensorTicker.attach(1, updateSensorData);
}

void loop() {
  checkButtonReset();
  server.handleClient();
  webSocket.loop();
  checkAlarmAndControlRelay();
  if (!mqttClient.connected()) {
    connectToMqtt();
  }
  mqttClient.loop();
}


void updateSensorData() {
  readDHT();
  readSoilMoisture();
  if (loggedIn) {
    notifyClients();
  }
  sendSensorDataMQTT();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  if (String(topic) == deviceCommandTopic) {
    bool newState = (message == "0");
    manualOverride = true;
    controlDevice(newState);
  }
}

void checkAlarmAndControlRelay() {
  timeClient.update();
  hora = timeClient.getHours();
  minuto = timeClient.getMinutes();
  segundo = timeClient.getSeconds();
  diaDaSemana = timeClient.getDay();

  bool dentroDoIntervalo = (hora > configHoraInicio || (hora == configHoraInicio && minuto >= configMinutoInicio))
                           && (hora < configHoraFim || (hora == configHoraFim && minuto <= configMinutoFim));

  if (!manualOverride) {
    if (dentroDoIntervalo && diasDaSemana[diaDaSemana]) {
      controlDevice(true);
    } else {
      controlDevice(false);
    }
  }
}

// Função principal para controlar o relé e atualiza o estado do rele no web server, mqtt e na memoria
void controlDevice(bool state) {
  digitalWrite(RELAY_PIN, state ? LOW : HIGH);
  preferences.putBool("relay", state);
  mqttClient.publish(deviceStateTopic, state ? "0" : "1");
  notifyClients();
}

// Envia os dados do estado do relé e dos sensores para a aplicação web.
void notifyClients() {
  String message = "{";
  message += "\"deviceState\":" + String(digitalRead(RELAY_PIN) == LOW ? "true" : "false") + ",";
  message += "\"temperature\":" + String(temperatura) + ",";
  message += "\"humidity\":" + String(umidade) + "}";
  webSocket.broadcastTXT(message);
}

void connectToMqtt() {
  if (!mqttClient.connected()) {
    Serial.print("Conectando ao MQTT...");
    if (mqttClient.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("Conectado");
    } else {
      Serial.print("Falha na conexão MQTT, rc=");
      Serial.println(mqttClient.state());
      // Não trava o loop, tenta reconectar periodicamente
    }
  }
}


void sendSensorDataMQTT() {
  mqttClient.publish(temperatureTopic, String(temperatura).c_str());
  mqttClient.publish(humidityTopic, String(umidade).c_str());
  mqttClient.publish(soilMoistureTopic, String(soilmoisture).c_str());
}

void setupServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/", HTTP_POST, handleConfigPost);
  server.on("/login", HTTP_GET, handleLogin);
  server.on("/login", HTTP_POST, handleLoginPost);
  server.on("/logout", HTTP_GET, handleLogout);
  server.onNotFound(handleNotFound);

  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void handleRoot() {
  if (!loggedIn) {
    server.sendHeader("Location", "/login");
    server.send(303);
    return;
  }

  String html = String(HOME_PAGE);
  updateHTMLPlaceholders(html);
  server.send(200, "text/html", html);
}

void handleConfigPost() {
  if (!loggedIn) {
    server.send(403, "text/plain", "Acesso negado");
    return;
  }

  if (server.hasArg("horaInicio") && server.arg("horaInicio") != "") {
    int novaHoraInicio = server.arg("horaInicio").toInt();
    if (novaHoraInicio >= 0 && novaHoraInicio < 24) {
      configHoraInicio = novaHoraInicio;
      preferences.putInt("horaInicio", configHoraInicio);
    }
  }

  if (server.hasArg("minutoInicio") && server.arg("minutoInicio") != "") {
    int novoMinutoInicio = server.arg("minutoInicio").toInt();
    if (novoMinutoInicio >= 0 && novoMinutoInicio < 60) {
      configMinutoInicio = novoMinutoInicio;
      preferences.putInt("minutoInicio", configMinutoInicio);
    }
  }

  if (server.hasArg("horaFim") && server.arg("horaFim") != "") {
    int novaHoraFim = server.arg("horaFim").toInt();
    if (novaHoraFim >= 0 && novaHoraFim < 24) {
      configHoraFim = novaHoraFim;
      preferences.putInt("horaFim", configHoraFim);
    }
  }

  if (server.hasArg("minutoFim") && server.arg("minutoFim") != "") {
    int novoMinutoFim = server.arg("minutoFim").toInt();
    if (novoMinutoFim >= 0 && novoMinutoFim < 60) {
      configMinutoFim = novoMinutoFim;
      preferences.putInt("minutoFim", configMinutoFim);
    }
  }

  for (int i = 0; i < 7; i++) {
    String diaKey = "dia" + String(i);
    diasDaSemana[i] = server.hasArg(diaKey);
    preferences.putBool(diaKey.c_str(), diasDaSemana[i]);
  }

  server.sendHeader("Location", "/");
  server.send(303);
}

void handleLogin() {
  server.send(200, "text/html", LOGIN_PAGE);
}

void handleLoginPost() {
  if (server.hasArg("username") && server.hasArg("password")) {
    if (server.arg("username") == defaultUsername && server.arg("password") == defaultPassword) {
      loggedIn = true;
      server.sendHeader("Set-Cookie", "session=1");
      server.sendHeader("Location", "/");
      server.send(303);
      return;
    }
  }
  server.send(401, "text/plain", "Login falhou");
}

void handleLogout() {
  loggedIn = false;
  server.sendHeader("Set-Cookie", "session=; Max-Age=0");
  server.sendHeader("Location", "/login");
  server.send(303);
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Não encontrado");
}

void updateHTMLPlaceholders(String& html) {
  html.replace("%TEMPERATURE%", String(temperatura, 1));
  html.replace("%HUMIDITY%", String(umidade, 1));
  html.replace("%SOIL_MOISTURE%", String(soilmoisture, 1));
  html.replace("%HORA_INICIO%", String(configHoraInicio));
  html.replace("%MINUTO_INICIO%", String(configMinutoInicio));
  html.replace("%HORA_FIM%", String(configHoraFim));
  html.replace("%MINUTO_FIM%", String(configMinutoFim));

  String diasSemanaHTML = "";
  for (int i = 0; i < 7; i++) {
    diasSemanaHTML += "<input type='checkbox' name='dia" + String(i) + "' " + (diasDaSemana[i] ? "checked" : "") + "> " + (i == 0 ? "D" : i == 1 ? "S"
                                                                                                                                        : i == 2 ? "T"
                                                                                                                                        : i == 3 ? "Q"
                                                                                                                                        : i == 4 ? "Q"
                                                                                                                                        : i == 5 ? "S"
                                                                                                                                                 : "S")
                      + "<br>";
  }
  html.replace("%DIAS_DA_SEMANA%", diasSemanaHTML);
}

// Recebe o controle do aplicativo Web Server Local
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) {
    String command = String((char*)payload);
    if (command == "toggle") {
      bool newState = !(digitalRead(RELAY_PIN) == LOW);
      manualOverride = true;    // Indica controle manual
      controlDevice(newState);  // Usa a função centralizada de controle
    }
  }
}