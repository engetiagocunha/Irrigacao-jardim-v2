#ifndef SENSORS_HPP
#define SENSORS_HPP

// Inclue as Bibliotecas necessárias
#include "library.hpp"

class Sensors {
public:
    Sensors();                  // Construtor padrão
    void begin();               // Inicializa os sensores
    void readSensors();         // Função para ler os sensores
    float getTemperature();      // Retorna a temperatura lida
    float getHumidity();         // Retorna a umidade lida
    int getSoilMoisture();       // Retorna a umidade do solo

private:
    DHT dht;                    // Instância do sensor DHT
    float temperature;          // Armazena a temperatura
    float humidity;             // Armazena a umidade
    int soilMoisture;          // Armazena a umidade do solo
};

#endif  // SENSORS_HPP
