#ifndef WIFI_SETUP_HPP
#define WIFI_SETUP_HPP

#include <ESPmDNS.h>      // Biblioteca para mDNS
#include <WiFiManager.h>  // Biblioteca para WiFiManager
#include <Preferences.h>  // Biblioteca para salvar configurações
#include "SetupGPIOs.hpp"

// Instâncias globais
WiFiManager wm;
extern Preferences preferences;  // Declaração externa da variável global

// Variáveis para configuração do MQTT
const char* mqtt_server = "192.168.15.129";
int mqtt_port = 1883;
const char* mqtt_user = "mqtt-user";
const char* mqtt_password = "1A2b3c4d";

// Parâmetros customizados do WiFiManager para MQTT
WiFiManagerParameter custom_mqtt_server("mqtt_server", "MQTT Server", mqtt_server, 40);
WiFiManagerParameter custom_mqtt_port("mqtt_port", "MQTT Port", String(mqtt_port).c_str(), 6);
WiFiManagerParameter custom_mqtt_user("mqtt_user", "MQTT User", mqtt_user, 20);
WiFiManagerParameter custom_mqtt_password("mqtt_password", "MQTT Password", mqtt_password, 20);

// Função para configurar o WiFiManager
void setupWiFi() {
  WiFi.mode(WIFI_STA);  // Define o modo Wi-Fi para estação (STA)
  Serial.println("\nIniciando configuração do WiFi...");

  // Carregar configurações salvas da memória
  preferences.begin("mqtt-config", false);
  mqtt_server = preferences.getString("server", mqtt_server).c_str();
  mqtt_port = preferences.getInt("port", mqtt_port);
  mqtt_user = preferences.getString("user", mqtt_user).c_str();
  mqtt_password = preferences.getString("password", mqtt_password).c_str();
  preferences.end();

  // Adicionar parâmetros customizados ao WiFiManager
  wm.addParameter(&custom_mqtt_server);
  wm.addParameter(&custom_mqtt_port);
  wm.addParameter(&custom_mqtt_user);
  wm.addParameter(&custom_mqtt_password);

  // Configuração do portal do WiFiManager
  bool res = wm.autoConnect("ESP3201-CONFIG", "12345678");
  if (!res) {
    Serial.println("Falha na conexão ou tempo limite esgotado");
  } else {
    Serial.println("Conectado com sucesso!");

    // Salvar configurações do MQTT após conexão
    preferences.begin("mqtt-config", false);
    preferences.putString("server", custom_mqtt_server.getValue());
    preferences.putInt("port", String(custom_mqtt_port.getValue()).toInt());
    preferences.putString("user", custom_mqtt_user.getValue());
    preferences.putString("password", custom_mqtt_password.getValue());
    preferences.end();

    // Atualizar variáveis globais
    mqtt_server = custom_mqtt_server.getValue();
    mqtt_port = String(custom_mqtt_port.getValue()).toInt();
    mqtt_user = custom_mqtt_user.getValue();
    mqtt_password = custom_mqtt_password.getValue();

    Serial.println("Configurações MQTT salvas:");
    Serial.println("Servidor: " + String(mqtt_server));
    Serial.println("Porta: " + String(mqtt_port));
    Serial.println("Usuário: " + String(mqtt_user));
    Serial.println("Senha: " + String(mqtt_password));
  }
}

// Função para configurar o mDNS
void setupMDNS() {
  if (!MDNS.begin("esphome")) {
    Serial.println("Erro ao configurar o mDNS!");
    while (1) {
      delay(1000);  // Loop infinito se falhar
    }
  }
  Serial.println("mDNS iniciado. Acesse esphome.local");
  MDNS.addService("http", "tcp", 80);  // Serviço HTTP via mDNS
}

// Função para verificar se o botão de reset foi pressionado
void checkButtonReset() {
  unsigned long buttonPressStartTime = 0;

  // Verifica se o botão foi pressionado
  if (digitalRead(RESET_BUTTON) == LOW) {
    Serial.println("Botão pressionado...");
    delay(50);  // Debounce
    if (digitalRead(RESET_BUTTON) == LOW) {
      buttonPressStartTime = millis();  // Salva o momento em que o botão foi pressionado

      // Mantém o loop enquanto o botão estiver pressionado
      while (digitalRead(RESET_BUTTON) == LOW) {
        unsigned long pressDuration = millis() - buttonPressStartTime;

        // Acessa o WiFi Manager após 3 segundos
        if (pressDuration >= 200 && pressDuration < 400) {
          Serial.println("Acessando as configurações do WiFi Manager...");
          wm.startConfigPortal();  // Abre o portal de configuração sem reiniciar
          return;
        }

        // Reseta o dispositivo após 4 segundos
        if (pressDuration >= 4000) {
          Serial.println("Apagando as configurações do WiFi e reiniciando...");
          wm.resetSettings();  // Limpa as configurações do WiFi
          ESP.restart();       // Reinicia o ESP32
        }
      }
    }
  }
}

#endif  // WIFI_SETUP_HPP
