#ifndef SETUPGPIOS_HPP
#define SETUPGPIOS_HPP

// Inclue as Bibliotecas necessárias
#include "library.hpp"

// Definição dos pinos de entrada e saída
#define RELAY_PIN_1 18
#define RELAY_PIN_2 19
#define RELAY_PIN_3 23
#define RELAY_PIN_4 5

#define TOUCH_PIN_1 13
#define TOUCH_PIN_2 12
#define TOUCH_PIN_3 14

#define RESET_BUTTON 26  // Botão de reset rede WiFi

#define DHTPIN 27
#define SOIL_SENSOR_PIN 36

// Defina o tipo de DHT que você está usando
#define DHTTYPE DHT11  // DHT 11

// Número de dispositivos e botões táteis
const int numDevices = 4;
const int numTouchButtons = 3;

// Arrays de pinos utilizando nos #define
extern const int devicePins[numDevices];
extern const int touchButtonPins[numTouchButtons];

//extern bool powerState[numDevices];
extern bool lastTouchStates[numTouchButtons];

extern const int capacitanceThreshold;
extern int touchMedians[numTouchButtons];

// Declaração da função de configuração dos GPIOs
void setupGPIOs();

#endif  // SETUPGPIOS_HPP
