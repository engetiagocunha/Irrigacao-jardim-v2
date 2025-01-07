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

// Definições de variáveis
Ticker sensorTicker;
Preferences preferences;

// Configurações do MQTT
const char* mqtt_server = "192.168.1.220";
const int mqtt_port = 1883;
const char* mqtt_user = "mqtt-user";
const char* mqtt_password = "1A2b3c4d";

// Tópicos MQTT para os relés
const char* device1CommandTopic = "/HaOS/esp01/device1/set";
const char* device2CommandTopic = "/HaOS/esp01/device2/set";
const char* device3CommandTopic = "/HaOS/esp01/device3/set";
const char* device4CommandTopic = "/HaOS/esp01/device4/set";

const char* device1StateTopic = "/HaOS/esp01/device1/state";
const char* device2StateTopic = "/HaOS/esp01/device2/state";
const char* device3StateTopic = "/HaOS/esp01/device3/state";
const char* device4StateTopic = "/HaOS/esp01/device4/state";

// Tópicos MQTT para os sensores
const char* temperatureTopic = "/HaOS/esp01/temperature";
const char* humidityTopic = "/HaOS/esp01/humidity";
const char* soilMoistureTopic = "/HaOS/esp01/soilMoisture";

WebServer server(80);
WebSocketsServer webSocket(81);
WiFiClient espClient;
PubSubClient mqttClient(espClient);
bool loggedIn = false;
const char* defaultUsername = "admin";
const char* defaultPassword = "admin";

// Variáveis de configurações do timer
int hora, minuto, segundo, diaDaSemana;
int configHoraInicio, configMinutoInicio, configHoraFim, configMinutoFim;
bool diasDaSemana[7] = { false, false, false, false, false, false, false };  // Domingo a Sábado

// Configuração do NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -10800, 60000);  // UTC-3 para Fortaleza (Nordeste)
//NTPClient timeClient(ntpUDP, "pool.ntp.org", -10800, 60000); // UTC-3 para Brasília e São Paulo
//NTPClient timeClient(ntpUDP, "pool.ntp.org", -14400, 60000); // UTC-4 para Manaus e Amazonas

// Função de configuração
void setup() {
  Serial.begin(115200);
  setupGPIOs();    // Configura GPIOs
  setupSensors();  // Inicializa o sensor DHT
  setupWiFi();     // Configura a conexão Wi-Fi
  setupMDNS();     // Configura a conexão mDNS

  // Inicializa Preferences e carrega as configurações
  preferences.begin("myApp", false);
  // Recupera estados salvos dos dispositivos
  for (int i = 0; i < numDevices; i++) {
    deviceStates[i] = preferences.getBool(("device" + String(i)).c_str(), false);
    digitalWrite(devicePins[i], deviceStates[i] ? LOW : HIGH);  // Aplica o estado salvo
  }

  // Carrega configurações com valores padrão
  configHoraInicio = preferences.getInt("horaInicio", 0);
  configMinutoInicio = preferences.getInt("minutoInicio", 0);
  configHoraFim = preferences.getInt("horaFim", 23);
  configMinutoFim = preferences.getInt("minutoFim", 59);

  for (int i = 0; i < 7; i++) {
    diasDaSemana[i] = preferences.getBool(("dia" + String(i)).c_str(), false);
  }

  // Configura o servidor web e WebSocket
  setupServer();
  timeClient.begin();

  // Configuração do MQTT
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
  connectToMqtt();

  // Calcula as medianas dos toques
  calculateTouchMedians();

  // Atualiza dados dos sensores a cada segundo
  sensorTicker.attach(1, updateSensorData);
}

// Função principal de loop
void loop() {
  checkButtonReset();
  server.handleClient();
  webSocket.loop();
  handleTouchButtons();  // Lida com os botões touch capacitivos
  checkAlarmAndControlRelay();

  mqttClient.loop();
}

// Função para atualizar os dados dos sensores
void updateSensorData() {
  readDHT();
  readSoilMoisture();
  if (loggedIn) {
    notifyClients();
  }
  sendSensorDataMQTT();  // Envia dados dos sensores via MQTT
}


// Configuração do servidor e WebSocket
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
  html.replace("%NUM_DEVICES%", String(numDevices));
  html.replace("%HORA_INICIO%", String(configHoraInicio));
  html.replace("%MINUTO_INICIO%", String(configMinutoInicio));
  html.replace("%HORA_FIM%", String(configHoraFim));
  html.replace("%MINUTO_FIM%", String(configMinutoFim));
  String diasSemanaHTML = "";
  for (int i = 0; i < 7; i++) {
    diasSemanaHTML += "<input type='checkbox' name='dia" + String(i) + "' " + (diasDaSemana[i] ? "checked" : "") + "> "
                      + (i == 0 ? "D" : i == 1 ? "S"
                                      : i == 2 ? "T"
                                      : i == 3 ? "Q"
                                      : i == 4 ? "Q"
                                      : i == 5 ? "S"
                                               : "S")  // Último caso, sem necessidade de verificação condicional
                      + "<br>";
  }
  html.replace("%DIAS_DA_SEMANA%", diasSemanaHTML);
}

// Evento WebSocket para controle de dispositivos via app web
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) {
    int index = atoi((char*)payload);  // Converte o payload para índice de dispositivo
    if (index >= 0 && index < numDevices) {
      bool newState = !deviceStates[index];  // Inverte o estado atual
      controlDevice(index, newState);        // Chama controlDevice com o novo estado
    }
  }
}

// Função para notificar o estado dos reles e dos sensores ao app web local
void notifyClients() {
  String message = "{\"deviceStates\":[";
  for (int i = 0; i < numDevices; i++) {
    message += deviceStates[i] ? "true" : "false";
    if (i < numDevices - 1) message += ",";
  }
  message += "],";
  message += "\"temperature\":" + String(temperatura) + ",";
  message += "\"humidity\":" + String(umidade) + "}";
  webSocket.broadcastTXT(message);
}

// Conectar ao Broker MQTT
void connectToMqtt() {
  while (!mqttClient.connected()) {
    Serial.print("Conectando ao MQTT...");
    if (mqttClient.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("Conectado");
      mqttClient.subscribe(device1CommandTopic);
      mqttClient.subscribe(device2CommandTopic);
      mqttClient.subscribe(device3CommandTopic);
      mqttClient.subscribe(device4CommandTopic);
    } else {
      Serial.print("Falha na conexão MQTT, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" Tentando novamente em 5 segundos");
      delay(5000);
    }
  }
}

// Função de callback do MQTT para controlar relés
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  bool state = (message == "0");  // `0` para ligar, `1` para desligar

  if (String(topic) == device1CommandTopic) {
    controlDevice(0, state);
  } else if (String(topic) == device2CommandTopic) {
    controlDevice(1, state);
  } else if (String(topic) == device3CommandTopic) {
    controlDevice(2, state);
  } else if (String(topic) == device4CommandTopic) {
    controlDevice(3, state);
  }
}

// Função para enviar dados dos sensores via MQTT
void sendSensorDataMQTT() {
  mqttClient.publish(temperatureTopic, String(temperatura).c_str());
  mqttClient.publish(humidityTopic, String(umidade).c_str());
  mqttClient.publish(soilMoistureTopic, String(soilmoisture).c_str());
}

// Função para monitorar a data e hora do alarme (Time)
void checkAlarmAndControlRelay() {
  timeClient.update();
  hora = timeClient.getHours();
  minuto = timeClient.getMinutes();
  segundo = timeClient.getSeconds();
  diaDaSemana = timeClient.getDay();

  Serial.print("Hora atual: ");
  Serial.print(hora);
  Serial.print(":");
  Serial.print(minuto);
  Serial.print(":");
  Serial.println(segundo);

  bool dentroDoIntervalo = (hora > configHoraInicio || (hora == configHoraInicio && minuto >= configMinutoInicio)) && (hora < configHoraFim || (hora == configHoraFim && minuto <= configMinutoFim));

  // Controle apenas do relé 1 com a condição de intervalo de tempo e dia da semana
  if (deviceStates[0]) {
    digitalWrite(devicePins[0], LOW);  // Relé 1 ligado manualmente
  } else if (dentroDoIntervalo && diasDaSemana[diaDaSemana]) {
    digitalWrite(devicePins[0], LOW);  // Relé 1 ligado automaticamente
  } else {
    digitalWrite(devicePins[0], HIGH);  // Relé 1 desligado
  }
  preferences.putBool("device0", deviceStates[0]);  // Salva o estado do relé 1
}


// Função para leitura e controle dos botões touch
void handleTouchButtons() {
  for (int i = 0; i < numTouchButtons; i++) {
    int touchValue = touchRead(touchButtonPins[i]);
    if (touchValue < touchMedians[i] - capacitanceThreshold && lastTouchStates[i] == HIGH) {
      if (i == 0) {
        deviceStates[0] = !deviceStates[0];  // Controle manual do relé 1
      } else {
        deviceStates[i] = !deviceStates[i];
      }
      digitalWrite(devicePins[i], deviceStates[i] ? LOW : HIGH);
      preferences.putBool(("device" + String(i)).c_str(), deviceStates[i]);
      notifyClients();
    }
    lastTouchStates[i] = (touchValue < touchMedians[i] - capacitanceThreshold) ? LOW : HIGH;
  }
}

void calculateTouchMedians() {
  for (int i = 0; i < numTouchButtons; i++) {
    int sum = 0;
    for (int j = 0; j < 100; j++) {
      sum += touchRead(touchButtonPins[i]);
    }
    touchMedians[i] = sum / 100;
  }
}


// Função de controle para dispositivos
void controlDevice(int deviceIndex, bool state) {
  switch (deviceIndex) {
    case 0:
      deviceStates[0] = state;
      digitalWrite(devicePins[0], state ? LOW : HIGH);
      preferences.putBool("device0", state);
      mqttClient.publish(device1StateTopic , state ? "0" : "1");
      break;
    case 1:
      deviceStates[1] = state;
      digitalWrite(devicePins[1], state ? LOW : HIGH);
      preferences.putBool("device1", state);
      mqttClient.publish(device2StateTopic , state ? "0" : "1");
      break;
    case 2:
      deviceStates[2] = state;
      digitalWrite(devicePins[2], state ? LOW : HIGH);
      preferences.putBool("device2", state);
      mqttClient.publish(device3StateTopic , state ? "0" : "1");
      break;
    case 3:
      deviceStates[3] = state;
      digitalWrite(devicePins[3], state ? LOW : HIGH);
      preferences.putBool("device3", state);
      mqttClient.publish(device4StateTopic , state ? "0" : "1");
      break;
    default:
      Serial.println("Dispositivo não encontrado");
  }
  notifyClients();  // Atualiza os clientes WebSocket
}
