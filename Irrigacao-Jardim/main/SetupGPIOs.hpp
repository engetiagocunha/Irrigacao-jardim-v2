#ifndef SETUPGPIOS_HPP
#define SETUPGPIOS_HPP

#include <Arduino.h>

// Definição dos pinos de entrada e saída
#define RELAY_PIN 15
#define BUTTON_PIN 18
#define RESET_BUTTON 23  
#define SOIL_SENSOR_PIN 26
#define DHTPIN 27

#define BAUD_RATE 115200

// Descomentar o modelo sensor a ser usado
#define DHTTYPE DHT11  // DHT 11
//#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

bool POWER_STATE = HIGH; //Inicia relé desligado

// Função para configurar os pinos como INPUT e OUTPUT
void setupGPIOs() {
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(RESET_BUTTON, INPUT_PULLUP);
  pinMode(SOIL_SENSOR_PIN, INPUT);

  //digitalWrite(RELAY_PIN, POWER_STATE);
}



#endif  // SETUPGPIOS_HPP