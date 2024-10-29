// Inclue as Bibliotecas necessárias
#include "setupGPIOS.hpp"

// Inicialização dos arrays e variáveis globais
const int devicePins[numDevices] = { RELAY_PIN_1, RELAY_PIN_2, RELAY_PIN_3, RELAY_PIN_4 };
const int touchButtonPins[numTouchButtons] = { TOUCH_PIN_1, TOUCH_PIN_2, TOUCH_PIN_3 };

bool deviceStates[numDevices] = { HIGH, HIGH, HIGH, HIGH };
bool lastTouchStates[numTouchButtons] = { LOW, LOW, LOW };

const int capacitanceThreshold = 30;
int touchMedians[numTouchButtons] = { 0, 0, 0 };

// Implementação da função de configuração dos pinos como INPUT e OUTPUT
void setupGPIOs() {
  for (int i = 0; i < numDevices; i++) {
    pinMode(devicePins[i], OUTPUT);
    digitalWrite(devicePins[i], HIGH);  // Inicializa os relés desligados
  }

  for (int i = 0; i < numTouchButtons; i++) {
    pinMode(touchButtonPins[i], INPUT);  // Configura os botões como entrada (se aplicável)
  }

  pinMode(RESET_BUTTON, INPUT_PULLUP);
  pinMode(SOIL_SENSOR_PIN, INPUT);
}
