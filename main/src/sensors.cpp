// Inclue as Bibliotecas necessárias
#include "sensors.hpp"

// Construtor padrão
Sensors::Sensors()
  : dht(DHTPIN, DHTTYPE), temperature(0), humidity(0), soilMoisture(0) {}

// Inicializa os sensores
void Sensors::begin() {
  dht.begin();  // Inicializa o sensor DHT
}

// Lê os valores dos sensores
void Sensors::readSensors() {
  // Leitura do DHT11
  temperature = dht.readTemperature();  // Lê a temperatura
  humidity = dht.readHumidity();        // Lê a umidade

  // Verifica se as leituras falharam
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Falha ao ler do DHT!");
    return;
  }

  // Lê o valor do sensor de umidade do solo
  int soilMoistureValue = analogRead(SOIL_SENSOR_PIN);     // Lê o valor analógico do sensor
  soilMoisture = map(soilMoistureValue, 1023, 0, 0, 100);  // Mapeia o valor para porcentagem (0% a 100%)
}

// Retorna a temperatura lida
float Sensors::getTemperature() {
  return temperature;
}

// Retorna a umidade lida
float Sensors::getHumidity() {
  return humidity;
}

// Retorna a umidade do solo
int Sensors::getSoilMoisture() {
  return soilMoisture;
}
