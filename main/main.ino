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

const char* relayKeys[4] = { "relay1", "relay2", "relay3", "relay4" };  // Chaves para os relés
int powerState[4] = { 1, 1, 1, 1 };                                     // Supondo que os relés estão desligados inicialmente

// Função para salvar o estado dos relés
void saveRelayState() {
  for (int i = 0; i < 4; i++) {
    preferences.putInt(relayKeys[i], powerState[i]);
  }
}

// Função para carregar o estado dos relés
void loadRelayState() {
  for (int i = 0; i < 4; i++) {
    powerState[i] = preferences.getInt(relayKeys[i], 1);             // Padrão: 1 (desligado)
    digitalWrite(RELAY_PIN_1 + i, powerState[i] == 0 ? LOW : HIGH);  // Define o estado inicial
  }
}

// Função para controlar os dispositivos
void deviceControl(int opc, char command) {
  // Verifica se o comando é o mesmo que o estado atual
  if ((command == '0' && powerState[opc - 1] == 0) || (command == '1' && powerState[opc - 1] == 1)) {
    return;
  }

  switch (opc) {
    case 1:
      // Lógica para o Relé 1
      digitalWrite(RELAY_PIN_1, (command == '0') ? LOW : HIGH);
      powerState[0] = (command == '0') ? 0 : 1;  // Atualiza o estado do relé

      // Publica o estado atualizado
      client.publish(device1StateTopic, (command == '0') ? "0" : "1");
      Serial.printf("DEVICE1 %s\n", (command == '0') ? "Ligado" : "Desligado");
      break;

    case 2:
      // Lógica para o Relé 2
      digitalWrite(RELAY_PIN_2, (command == '0') ? LOW : HIGH);
      powerState[1] = (command == '0') ? 0 : 1;  // Atualiza o estado do relé

      // Publica o estado atualizado
      client.publish(device2StateTopic, (command == '0') ? "0" : "1");
      Serial.printf("DEVICE2 %s\n", (command == '0') ? "Ligado" : "Desligado");
      break;

    case 3:
      // Lógica para o Relé 3
      digitalWrite(RELAY_PIN_3, (command == '0') ? LOW : HIGH);
      powerState[2] = (command == '0') ? 0 : 1;  // Atualiza o estado do relé

      // Publica o estado atualizado
      client.publish(device3StateTopic, (command == '0') ? "0" : "1");
      Serial.printf("DEVICE3 %s\n", (command == '0') ? "Ligado" : "Desligado");
      break;

    case 4:
      // Lógica para o Relé 4
      digitalWrite(RELAY_PIN_4, (command == '0') ? LOW : HIGH);
      powerState[3] = (command == '0') ? 0 : 1;  // Atualiza o estado do relé

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
      client.publish(device1StateTopic, (powerState[0] ? "1" : "0"));
      client.publish(device2StateTopic, (powerState[1] ? "1" : "0"));
      client.publish(device3StateTopic, (powerState[2] ? "1" : "0"));
      client.publish(device4StateTopic, (powerState[3] ? "1" : "0"));
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
  client.publish("/HaOS/esp01/device1/state", powerState[0] == 0 ? "0" : "1");
  client.publish("/HaOS/esp01/device2/state", powerState[1] == 0 ? "0" : "1");
  client.publish("/HaOS/esp01/device3/state", powerState[2] == 0 ? "0" : "1");
  client.publish("/HaOS/esp01/device4/state", powerState[3] == 0 ? "0" : "1");
  Serial.println("Estados dos dispositivos publicados.");
}

void setup() {
  Serial.begin(115200);
  setupGPIOs();     // Configura os pinos GPIO
  setupWiFi();      // Configura a conexão WiFi
  sensors.begin();  // Inicializa os sensores

  preferences.begin("relay", false);  // Inicializa a biblioteca Preferences
  loadRelayState();                   // Carrega o estado dos relés

  // Inicia o Ticker para atualizar os sensores a cada 10 segundos
  sensorTicker.attach(10, updateSensors);  // Chama updateSensors a cada 10 segundos
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

  // Salva o estado dos relés sempre que houver uma mudança
  static int lastPowerState[4] = { 1, 1, 1, 1 };
  for (int i = 0; i < 4; i++) {
    if (powerState[i] != lastPowerState[i]) {
      saveRelayState();
      lastPowerState[i] = powerState[i];
    }
  }
}
