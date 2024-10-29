// Inclui as Bibliotecas necessárias
#include "src/library.hpp"

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


WiFiClient espClient;
PubSubClient client(espClient);

Preferences preferences;   // Instância da classe Preferences
Sensors sensors;           // Instância da classe Sensors
Ticker sensorTicker;       // Instância do Ticker
Ticker stateUpdateTicker;  // Instancia o Ticker


// Função para controlar os dispositivos
void deviceControl(int opc, char command) {
  // Verifica se o comando é o mesmo que o estado atual
  if ((command == '0' && deviceStates[opc - 1] == 0) || (command == '1' && deviceStates[opc - 1] == 1)) {
    return;
  }

  switch (opc) {
    case 1:
      // Lógica para o Relé 1
      digitalWrite(devicePins[0], (command == '0') ? LOW : HIGH);
      deviceStates[0] = (command == '0') ? false : true;  // Atualiza o estado do dispositivo
      preferences.putBool("device0", deviceStates[0]);    // Salva o estado
      // Publica o estado atualizado
      client.publish(device1StateTopic, (command == '0') ? "0" : "1");
      Serial.printf("DEVICE1 %s\n", (command == '0') ? "Ligado" : "Desligado");
      break;

    case 2:
      // Lógica para o Relé 2
      digitalWrite(devicePins[1], (command == '0') ? LOW : HIGH);
      deviceStates[1] = (command == '0') ? false : true;  // Atualiza o estado do dispositivo
      preferences.putBool("device1", deviceStates[1]);    // Salva o estado
      // Publica o estado atualizado
      client.publish(device2StateTopic, (command == '0') ? "0" : "1");
      Serial.printf("DEVICE2 %s\n", (command == '0') ? "Ligado" : "Desligado");
      break;

    case 3:
      // Lógica para o Relé 3
      digitalWrite(devicePins[2], (command == '0') ? LOW : HIGH);
      deviceStates[2] = (command == '0') ? false : true;  // Atualiza o estado do dispositivo
      preferences.putBool("device2", deviceStates[2]);    // Salva o estado
      // Publica o estado atualizado
      client.publish(device3StateTopic, (command == '0') ? "0" : "1");
      Serial.printf("DEVICE3 %s\n", (command == '0') ? "Ligado" : "Desligado");
      break;

    case 4:
      // Lógica para o Relé 4
      digitalWrite(devicePins[3], (command == '0') ? LOW : HIGH);
      deviceStates[3] = (command == '0') ? false : true;  // Atualiza o estado do dispositivo
      preferences.putBool("device3", deviceStates[3]);    // Salva o estado
      // Publica o estado atualizado
      client.publish(device4StateTopic, (command == '0') ? "0" : "1");
      Serial.printf("DEVICE4 %s\n", (command == '0') ? "Ligado" : "Desligado");
      break;

    default:
      Serial.println("Opção inválida");
      break;  // Retorna se a opção não for válida
  }
}


// Callback para receber comandos do MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  char command = (char)payload[0];
  Serial.printf("Mensagem recebida [%s]: %c\n", topic, command);

  if (strcmp(topic, device1CommandTopic) == 0) {
    deviceControl(1, command);
  } else if (strcmp(topic, device2CommandTopic) == 0) {
    deviceControl(2, command);
  } else if (strcmp(topic, device3CommandTopic) == 0) {
    deviceControl(3, command);
  } else if (strcmp(topic, device4CommandTopic) == 0) {
    deviceControl(4, command);
  }
}

// Função para reconectar ao MQTT
void reconnect() {
  while (!client.connected()) {
    Serial.print("Tentando conexão MQTT...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
      Serial.println("conectado");

      // Inscreve nos tópicos de comando
      client.subscribe(device1CommandTopic);
      client.subscribe(device2CommandTopic);
      client.subscribe(device3CommandTopic);
      client.subscribe(device4CommandTopic);

      // Publica o estado inicial
      client.publish(device1StateTopic, (deviceStates[0] ? "1" : "0"));
      client.publish(device2StateTopic, (deviceStates[1] ? "1" : "0"));
      client.publish(device3StateTopic, (deviceStates[2] ? "1" : "0"));
      client.publish(device4StateTopic, (deviceStates[3] ? "1" : "0"));
    } else {
      Serial.printf("falhou, rc=%d tentando novamente em 5 segundos\n", client.state());
      delay(5000);
    }
  }
}

// Função que será chamada pelo Ticker para atualizar os sensores
void updateSensors() {
  sensors.readSensors();  // Lê os sensores

  // Publica os dados dos sensores
  client.publish(temperatureTopic, String(sensors.getTemperature()).c_str());
  client.publish(humidityTopic, String(sensors.getHumidity()).c_str());
  client.publish(soilMoistureTopic, String(sensors.getSoilMoisture()).c_str());
}

// Função para publicar o estado de cada dispositivo
void publishStates() {
  client.publish("/HaOS/esp01/device1/state", deviceStates[0] == 0 ? "0" : "1");
  client.publish("/HaOS/esp01/device2/state", deviceStates[1] == 0 ? "0" : "1");
  client.publish("/HaOS/esp01/device3/state", deviceStates[2] == 0 ? "0" : "1");
  client.publish("/HaOS/esp01/device4/state", deviceStates[3] == 0 ? "0" : "1");
  Serial.println("Estados dos dispositivos publicados.");
}

void setup() {
  Serial.begin(115200);
  setupGPIOs();     // Configura os pinos GPIO
  setupWiFi();      // Configura a conexão WiFi
  sensors.begin();  // Inicializa os sensores

  preferences.begin("deviceStates", false);  // Inicializa a biblioteca Preferences
  for (int i = 0; i < numDevices; i++) {
    deviceStates[i] = preferences.getBool(String("device" + String(i)).c_str(), false);
    digitalWrite(devicePins[i], deviceStates[i] ? LOW : HIGH);  // Aplica o estado salvo
  }                                                             

  // Inicia o Ticker para atualizar os sensores a cada 10 segundos
  sensorTicker.attach(10, updateSensors);      // Chama updateSensors a cada 10 segundos
  stateUpdateTicker.attach(5, publishStates);  // Intervalo em segundos

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  checkButtonReset();  // Verifica o botão de reset
  if (!client.connected()) {
    reconnect();  // Tenta reconectar ao MQTT se desconectado
  }
  client.loop();  // Mantém a conexão e processa mensagens
}
