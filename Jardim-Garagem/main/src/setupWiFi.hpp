#ifndef SETUPWIFI_HPP
#define SETUPWIFI_HPP

// Inclue as Bibliotecas necessárias
#include "library.hpp"

extern WiFiManager wm;  // Instância global do WiFiManager

// Declaração das funções de configuração WiFi e mDNS
void setupWiFi();
//void setupMDNS();
void checkButtonReset();

#endif  // SETUPWIFI_HPP
