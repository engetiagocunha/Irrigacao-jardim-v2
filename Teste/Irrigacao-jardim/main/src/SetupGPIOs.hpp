#ifndef SETUPGPIOS_HPP
#define SETUPGPIOS_HPP

#include <Arduino.h>

// Definição dos pinos de entrada e saída
#define RELAY_PIN 15
#define RESET_BUTTON 05  // Botão de reset rede WiFi
#define DHTPIN 13
#define SOIL_SENSOR_PIN 12

// Descomente qualquer tipo que você esteja usando!
#define DHTTYPE DHT11  // DHT 11
//#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)


// Função para configurar os pinos como INPUT e OUTPUT
void setupGPIOs() {
  // Configura o dispositivo (relés) como saída
  pinMode(RELAY_PIN, OUTPUT);

  // Configura o botão de reset como entrada com resistor interno PULLUP
  pinMode(RESET_BUTTON, INPUT_PULLUP);

  // Configura o pino do sensor de umidade de solo
  pinMode(SOIL_SENSOR_PIN, INPUT);

}

#endif  // SETUPGPIOS_HPP