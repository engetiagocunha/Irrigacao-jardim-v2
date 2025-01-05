#ifndef SENSORS_HPP
#define SENSORS_HPP

#include <DHT.h>  // Inclui a biblioteca DHT
#include "SetupGPIOs.hpp"

// Definições das variáveis globais
float temperatura;
float umidade;

float soilMoistureValue = 0;  // Armazena o valor lido do sensor
float soilmoisture;

DHT dht(DHTPIN, DHTTYPE);

// Funções para inicializar e ler o sensor DHT
void setupSensors() {
  dht.begin();  // Inicializa o sensor DHT com o pino e o tipo definidos
}

void readDHT() {
  umidade = dht.readHumidity();         // Lê a umidade
  temperatura = dht.readTemperature();  // Lê a temperatura em Celsius
  if (isnan(temperatura) || isnan(umidade)) {
    Serial.println("Erro ao ler o DHT!");
    temperatura = 0;
    umidade = 0;
  }
}

// Função para ler o sensor de umidade do solo
void readSoilMoisture() {
  soilMoistureValue = analogRead(SOIL_SENSOR_PIN);         // Lê o valor analógico do sensor
  soilmoisture = map(soilMoistureValue, 1023, 0, 0, 100);  // Mapeia o valor para uma porcentagem de 0% a 100%
}

#endif  // SENSORS_HPP