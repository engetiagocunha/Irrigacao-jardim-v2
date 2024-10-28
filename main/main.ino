#include <WiFi.h>
#include <PubSubClient.h>

// Definição dos pinos dos relés
#define RELAY_PIN_1 18
#define RELAY_PIN_2 19
#define RELAY_PIN_3 23
#define RELAY_PIN_4 05

// Estados dos dispositivos
int powerState[4] = { 1, 1, 1, 1 };

// Configurações do WiFi
const char* ssid = "E5TR3L1Nh4&M3L 2";
const char* password = "Kx8mrWQByh";

// Configurações do MQTT
const char* mqtt_server = "192.168.1.220";
const int mqtt_port = 1883;
const char* mqtt_user = "mqtt-user";
const char* mqtt_password = "1A2b3c4d";

// Tópicos MQTT
const char* device1CommandTopic = "/HaOS/esp01/device1/set";
const char* device2CommandTopic = "/HaOS/esp01/device2/set";
const char* device3CommandTopic = "/HaOS/esp01/device3/set";
const char* device4CommandTopic = "/HaOS/esp01/device4/set";

const char* device1StateTopic = "/HaOS/esp01/device1/state";
const char* device2StateTopic = "/HaOS/esp01/device2/state";
const char* device3StateTopic = "/HaOS/esp01/device3/state";
const char* device4StateTopic = "/HaOS/esp01/device4/state";

WiFiClient espClient;
PubSubClient client(espClient);

// Função deviceControl
void deviceControl(int opc, char command) {
  switch (opc) {
    case 1:
      if (command == '1' && powerState[0] == 1) {
        digitalWrite(RELAY_PIN_1, LOW);
        powerState[0] = 0;
        client.publish(device1StateTopic, "1");
        Serial.println("DEVICE1 Ligado");
      } else if (command == '0' && powerState[0] == 0) {
        digitalWrite(RELAY_PIN_1, HIGH);
        powerState[0] = 1;
        client.publish(device1StateTopic, "0");
        Serial.println("DEVICE1 Desligado");
      }
      break;

    case 2:
      if (command == '1' && powerState[1] == 1) {
        digitalWrite(RELAY_PIN_2, LOW);
        powerState[1] = 0;
        client.publish(device2StateTopic, "1");
        Serial.println("DEVICE2 Ligado");
      } else if (command == '0' && powerState[1] == 0) {
        digitalWrite(RELAY_PIN_2, HIGH);
        powerState[1] = 1;
        client.publish(device2StateTopic, "0");
        Serial.println("DEVICE2 Desligado");
      }
      break;

    case 3:
      if (command == '1' && powerState[2] == 1) {
        digitalWrite(RELAY_PIN_3, LOW);
        powerState[2] = 0;
        client.publish(device3StateTopic, "1");
        Serial.println("DEVICE3 Ligado");
      } else if (command == '0' && powerState[2] == 0) {
        digitalWrite(RELAY_PIN_3, HIGH);
        powerState[2] = 1;
        client.publish(device3StateTopic, "0");
        Serial.println("DEVICE3 Desligado");
      }
      break;

    case 4:
      if (command == '1' && powerState[3] == 1) {
        digitalWrite(RELAY_PIN_4, LOW);
        powerState[3] = 0;
        client.publish(device4StateTopic, "1");
        Serial.println("DEVICE4 Ligado");
      } else if (command == '0' && powerState[3] == 0) {
        digitalWrite(RELAY_PIN_4, HIGH);
        powerState[3] = 1;
        client.publish(device4StateTopic, "0");
        Serial.println("DEVICE4 Desligado");
      }
      break;
  }
}


void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.println("Endereço IP: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  char command = (char)payload[0];
  Serial.print("Mensagem recebida [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(command);

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

      // Publica estado inicial
      client.publish(device1StateTopic, (powerState[0] ? "1" : "0"));
      client.publish(device2StateTopic, (powerState[1] ? "1" : "0"));
      client.publish(device3StateTopic, (powerState[2] ? "1" : "0"));
      client.publish(device4StateTopic, (powerState[3] ? "1" : "0"));
    } else {
      Serial.print("falhou, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5 segundos");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN_1, OUTPUT);
  pinMode(RELAY_PIN_2, OUTPUT);
  pinMode(RELAY_PIN_3, OUTPUT);
  pinMode(RELAY_PIN_4, OUTPUT);

  digitalWrite(RELAY_PIN_1, HIGH);
  digitalWrite(RELAY_PIN_2, HIGH);
  digitalWrite(RELAY_PIN_3, HIGH);
  digitalWrite(RELAY_PIN_4, HIGH);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  delay(10);  // Reduz carga de processamento
}
