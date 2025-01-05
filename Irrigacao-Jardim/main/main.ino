#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Arduino.h>
#include <Preferences.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Ticker.h>
#include "SetupGPIOs.hpp"
#include "SetupWiFi.hpp"
#include "Sensors.hpp"

// Inicialização do LCD com o endereço 0x27 e o tamanho do display (16 colunas e 2 linhas)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Configuração do NTP Client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -10800, 60000);  // UTC-3 para Fortaleza (Nordeste)

// Definição de variáveis auxiliares
bool systemOn = false;      // Estado do sistema (ligado/desligado)
unsigned long buttonPressTime = 0;
bool buttonHeld = false;
int displayMode = 0;        // 0: Temp/Umidade, 1: Umidade Solo, 2: Hora, 3: WiFi

void setup() {
  Serial.begin(BAUD_RATE);
  setupGPIOs();    // Configura GPIOs
  setupSensors();  // Inicializa o sensor DHT
  setupWiFi();     // Configura a conexão Wi-Fi
  setupMDNS();     // Configura a conexão mDNS
  timeClient.begin();  // Inicializa o NTP Client

  // Inicializa o display LCD
  lcd.begin(16, 2);
  lcd.backlight();      // Liga a luz de fundo do LCD
  lcd.setCursor(0, 0);  // Posiciona o cursor na primeira linha e coluna
  lcd.print("Sistema Off");
}

void loop() {
  // Verifica o botão para ligar/desligar o sistema ou alternar o modo
  handleButton();

  if (systemOn) {
    // Exibe informações de acordo com o modo selecionado
    switch (displayMode) {
      case 0: // Exibir temperatura e umidade
        readDHT();
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Temp: " + String(temperatura, 1) + "C");
        lcd.setCursor(0, 1);
        lcd.print("Umid: " + String(umidade, 1) + "%");
        break;

      case 1: // Exibir umidade do solo
        readSoilMoisture();
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Umid. Solo:");
        lcd.setCursor(0, 1);
        lcd.print(String(soilmoisture, 1) + "%");
        break;

      case 2: // Exibir hora atual
        timeClient.update();
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Hora Atual:");
        lcd.setCursor(0, 1);
        lcd.print(timeClient.getFormattedTime());
        break;

      case 3: // Exibir nome da rede Wi-Fi conectada
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("WiFi:");
        lcd.setCursor(0, 1);
        lcd.print(WiFi.SSID());
        break;
    }
  } else {
    // Sistema desligado: exibe hora atual
    timeClient.update();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sistema Off");
    lcd.setCursor(0, 1);
    lcd.print(timeClient.getFormattedTime());
  }

  delay(500);  // Atualização a cada meio segundo
}

void handleButton() {
  if (digitalRead(BUTTON_PIN) == LOW) {  // Botão pressionado
    if (!buttonHeld) {
      buttonPressTime = millis();
      buttonHeld = true;
    } else if (millis() - buttonPressTime > 2000) {  // Segurou por 2 segundos
      systemOn = !systemOn;  // Alterna entre ligado/desligado
      displayMode = 0;       // Reseta para o modo inicial
      buttonHeld = false;

      lcd.clear();
      if (systemOn) {
        lcd.setCursor(0, 0);
        lcd.print("Sistema Ligado");
      } else {
        lcd.setCursor(0, 0);
        lcd.print("Sistema Off");
      }
      delay(1000);  // Aguarda 1 segundo antes de continuar
    }
  } else if (buttonHeld) {  // Botão foi liberado antes dos 2 segundos
    buttonHeld = false;
    if (systemOn) {
      displayMode = (displayMode + 1) % 4;  // Alterna entre os modos 0, 1, 2, 3
    }
  }
}
