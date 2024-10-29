// Inclue as Bibliotecas necessárias
#include "setupWiFi.hpp"

WiFiManager wm;  // Instância global do WiFiManager

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  Serial.println("\nIniciando configuração do WiFi...");

  bool wm_nonblocking = false;
  if (wm_nonblocking) wm.setConfigPortalBlocking(false);

  std::vector<const char*> menu = { "wifi", "info", "exit" };
  wm.setMenu(menu);
  wm.setClass("invert");
  wm.setConfigPortalTimeout(30);

  bool res = wm.autoConnect("ESP3201-CONFIG", "12345678");
  if (!res) {
    Serial.println("Falha na conexão ou tempo limite esgotado");
  } else {
    Serial.println("Conectado com sucesso!");
  }
}

/*void setupMDNS() {
  if (!MDNS.begin("esphome")) {
    Serial.println("Erro ao configurar o mDNS!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS iniciado. Acesse esphome.local");
  MDNS.addService("http", "tcp", 80);
}*/

void checkButtonReset() {
  unsigned long buttonPressStartTime = 0;

  if (digitalRead(RESET_BUTTON) == LOW) {
    Serial.println("Botão pressionado...");
    delay(50);
    if (digitalRead(RESET_BUTTON) == LOW) {
      buttonPressStartTime = millis();

      while (digitalRead(RESET_BUTTON) == LOW) {
        unsigned long pressDuration = millis() - buttonPressStartTime;

        if (pressDuration >= 2000 && pressDuration < 4000) {
          Serial.println("Acessando as configurações do WiFi Manager...");
          wm.startConfigPortal();
          return;
        }

        if (pressDuration >= 4000) {
          Serial.println("Apagando as configurações do WiFi e reiniciando...");
          wm.resetSettings();
          ESP.restart();
        }
      }
    }
  }
}
