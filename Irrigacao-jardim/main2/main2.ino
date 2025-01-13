// Bibliotecas
#include <ESPmDNS.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <Preferences.h>
#include <Ticker.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Definições de pinos e constantes
namespace Config {
// Pinos
constexpr uint8_t DHTPIN = 13;
constexpr uint8_t NUM_SOIL_SENSORS = 2;
constexpr uint8_t SOIL_SENSOR1 = 12;
constexpr uint8_t SOIL_SENSOR2 = 14;
constexpr uint8_t RELAY_PIN = 15;
constexpr uint8_t BTN_MODE = 27;
constexpr uint8_t BTN_DISPLAY = 26;

// Timeouts e intervalos
constexpr unsigned long DISPLAY_TIMEOUT = 60000;
constexpr unsigned long LONG_PRESS_MODE = 2000;
constexpr unsigned long LONG_PRESS_DISPLAY = 5000;
constexpr unsigned long TEMP_MESSAGE_DURATION = 1500;
constexpr unsigned long DURACAO_MAXIMA_IRRIGACAO = 20 * 60 * 1000;

// Limites de irrigação
constexpr float SOIL_MOISTURE_MIN = 40.0;
constexpr float SOIL_MOISTURE_MAX = 100.0;
}

// Estruturas de dados
struct SystemState {
  bool agendamentoAtivo = false;
  bool modoAutomatico = true;
  bool relayStatus = false;
  int displayMode = 0;
  float temperatura = 0;
  float umidade = 0;
  float umidadeSolo = 0;
};

struct ButtonControl {
  unsigned long modoButtonStart = 0;
  unsigned long displayButtonStart = 0;
  unsigned long lastButtonTime = 0;
  unsigned long messageStartTime = 0;
  bool modoButtonPressed = false;
  bool displayButtonPressed = false;
  bool displayBacklight = false;
  bool showingMessage = false;
  bool modoResetWiFi = false;
};

struct AgendamentoConfig {
  int horaAtual = 0;
  int minutoAtual = 0;
  int diaSemanaAtual = 0;
  int horaLigar = 0;
  int minutoLigar = 0;
  bool diasSemana[7] = { true };
};

// Variáveis globais
WiFiManager wm;
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(Config::DHTPIN, DHT11);
Preferences preferences;
Ticker sensorTicker;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -10800, 60000);

SystemState systemState;
ButtonControl buttonControl;
AgendamentoConfig agendamento;

const char* diasNomes[7] = { "Seg", "Ter", "Qua", "Qui", "Sex", "Sab", "Dom" };
unsigned long tempoIrrigacaoInicio = 0;
bool agendamentoJaExecutado = false;

// Classe para gerenciar o display LCD
class DisplayManager {
public:
  static void mostrarMensagemTemporaria(const char* mensagem) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(mensagem);
    buttonControl.showingMessage = true;
    buttonControl.messageStartTime = millis();
  }

  static void mostrarInfoWiFi() {
    int rssi = WiFi.RSSI();
    int signalStrength = map(constrain(rssi, -100, -50), -100, -50, 0, 100);

    criarCaracteresPersonalizados();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WIFI:");

    int numBlocks = map(signalStrength, 0, 100, 0, 5);
    for (int i = 0; i < numBlocks; i++) {
      lcd.setCursor(6 + i, 0);
      lcd.write(byte(i));
    }

    lcd.setCursor(0, 1);
    if (WiFi.status() == WL_CONNECTED) {
      lcd.print(WiFi.SSID());
      lcd.print(" ");
      lcd.print(signalStrength);
      lcd.print("%");
    } else {
      lcd.print("Desconectado");
    }
  }

private:
  static void criarCaracteresPersonalizados() {
    byte signalBlocks[5][8] = {
      { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1F },
      { 0x0, 0x0, 0x0, 0x0, 0x0, 0x1F, 0x1F, 0x1F },
      { 0x0, 0x0, 0x0, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F },
      { 0x0, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F },
      { 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F }
    };

    for (int i = 0; i < 5; i++) {
      lcd.createChar(i, signalBlocks[i]);
    }
  }
};

// Classe para gerenciar sensores
class SensorManager {
public:
  static void lerSensores() {
    systemState.temperatura = dht.readTemperature();
    systemState.umidade = dht.readHumidity();

    float soloRawTotal = 0;
    int sensoresValidos = 0;
    const int SOIL_SENSOR_PINS[Config::NUM_SOIL_SENSORS] = {
      Config::SOIL_SENSOR1,
      Config::SOIL_SENSOR2
    };

    for (int i = 0; i < Config::NUM_SOIL_SENSORS; i++) {
      int soloRaw = analogRead(SOIL_SENSOR_PINS[i]);
      if (soloRaw >= 0 && soloRaw <= 4095) {
        float soloPercentage = map(soloRaw, 4095, 0, 0, 100);
        soloRawTotal += soloPercentage;
        sensoresValidos++;
      }
    }

    systemState.umidadeSolo = (sensoresValidos > 0) ? (soloRawTotal / sensoresValidos) : 0;
  }
};

// Classe para gerenciar o controle de irrigação
class IrrigationController {
public:
  static void controleRele(bool estado) {
    digitalWrite(Config::RELAY_PIN, estado);
    systemState.relayStatus = estado;
    preferences.putBool("relay", estado);
  }

  static void verificarIrrigacaoAutomatica() {
    if (!systemState.modoAutomatico) return;

    bool horarioAgendado = verificarAgendamento();

    if (systemState.umidadeSolo < Config::SOIL_MOISTURE_MIN && horarioAgendado && !systemState.relayStatus) {
      controleRele(true);
      tempoIrrigacaoInicio = millis();
    }

    if (systemState.relayStatus) {
      if (systemState.umidadeSolo >= Config::SOIL_MOISTURE_MAX || (millis() - tempoIrrigacaoInicio >= Config::DURACAO_MAXIMA_IRRIGACAO)) {
        controleRele(false);
      }
    }
  }

private:
  static bool verificarAgendamento() {
    timeClient.update();
    agendamento.horaAtual = timeClient.getHours();
    agendamento.minutoAtual = timeClient.getMinutes();
    agendamento.diaSemanaAtual = timeClient.getDay();

    return agendamento.diasSemana[agendamento.diaSemanaAtual] && (agendamento.horaAtual == agendamento.horaLigar && agendamento.minutoAtual == agendamento.minutoLigar);
  }
};


// Classe para gerenciar WiFi
class WiFiSetup {
public:
  static void setup() {
    WiFi.mode(WIFI_STA);
    Serial.println("\nIniciando configuração do WiFi...");

    bool wm_nonblocking = true;
    if (wm_nonblocking) wm.setConfigPortalBlocking(false);

    std::vector<const char*> menu = { "wifi", "info", "param", "exit" };
    wm.setMenu(menu);
    wm.setClass("invert");
    wm.setConfigPortalTimeout(30);

    // Parâmetros de agendamento
    WiFiManagerParameter custom_hora("hora", "Hora para Ligar",
                                     String(agendamento.horaLigar).c_str(), 2);
    WiFiManagerParameter custom_minuto("minuto", "Minuto para Ligar",
                                       String(agendamento.minutoLigar).c_str(), 2);

    wm.addParameter(&custom_hora);
    wm.addParameter(&custom_minuto);

    // Checkboxes para dias da semana
    char htmlDias[500] = "";
    for (int i = 0; i < 7; i++) {
      char checkbox[100];
      snprintf(checkbox, sizeof(checkbox),
               "<label><input type='checkbox' name='dia%d' value='1' %s>%s</label><br>",
               i, agendamento.diasSemana[i] ? "checked" : "", diasNomes[i]);
      strcat(htmlDias, checkbox);
    }
    WiFiManagerParameter custom_dias("dias", "Dias da Semana", htmlDias, 500);
    wm.addParameter(&custom_dias);

    bool res = wm.autoConnect("ESP3201-CONFIG", "12345678");
    if (!res) {
      Serial.println("Falha na conexão ou tempo limite esgotado");
      return;
    }

    Serial.println("Conectado com sucesso!");

    if (wm.getConfigPortalActive()) {
      agendamento.horaLigar = atoi(custom_hora.getValue());
      agendamento.minutoLigar = atoi(custom_minuto.getValue());

      for (int i = 0; i < 7; i++) {
        char paramName[10];
        snprintf(paramName, sizeof(paramName), "dia%d", i);
        agendamento.diasSemana[i] = (wm.server->hasArg(paramName) && wm.server->arg(paramName) == "1");
      }

      ScheduleManager::salvarConfigAgendamento();
    }

    inicializarNTP();
  }

  static void reset() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Resetando WiFi ...");

    wm.resetSettings();
    delay(1000);
    ESP.restart();
  }

private:
  static void inicializarNTP() {
    timeClient.begin();
    timeClient.update();
  }
};


// Classe para gerenciar agendamento
class ScheduleManager {
public:
  static void salvarConfigAgendamento() {
    preferences.begin("agendamento", false);
    preferences.putInt("hora", agendamento.horaLigar);
    preferences.putInt("minuto", agendamento.minutoLigar);

    uint8_t diasSalvar = 0;
    for (int i = 0; i < 7; i++) {
      if (agendamento.diasSemana[i]) {
        diasSalvar |= (1 << i);
      }
    }
    preferences.putUChar("diasSemana", diasSalvar);

    preferences.end();
  }

  static void carregarConfigAgendamento() {
    preferences.begin("agendamento", true);
    agendamento.horaLigar = preferences.getInt("hora", 0);
    agendamento.minutoLigar = preferences.getInt("minuto", 0);

    uint8_t diasCarregados = preferences.getUChar("diasSemana", 0);
    for (int i = 0; i < 7; i++) {
      agendamento.diasSemana[i] = (diasCarregados & (1 << i)) != 0;
    }

    preferences.end();
  }

  static void atualizarDataHora() {
    timeClient.update();
    agendamento.horaAtual = timeClient.getHours();
    agendamento.minutoAtual = timeClient.getMinutes();
    agendamento.diaSemanaAtual = timeClient.getDay();
  }
};

// Classe para gerenciar botões e interface
class InterfaceManager {
public:
  static void verificarBotoes() {
    verificarBotaoMode();
    verificarBotaoDisplay();
  }

  static void atualizarDisplay() {
    static int ultimoDisplayMode = -1;
    static float ultimaTemperatura = -1;
    static float ultimaUmidade = -1;
    static float ultimaUmidadeSolo = -1;
    static bool ultimoModoAutomatico = false;
    static bool ultimoRelayStatus = false;

    if (buttonControl.showingMessage && (millis() - buttonControl.messageStartTime < Config::TEMP_MESSAGE_DURATION)) {
      return;
    }

    buttonControl.showingMessage = false;

    bool precisaAtualizar = verificarNecessidadeAtualizacao(
      ultimoDisplayMode, ultimaTemperatura, ultimaUmidade,
      ultimaUmidadeSolo, ultimoModoAutomatico, ultimoRelayStatus);

    if (precisaAtualizar) {
      atualizarDisplayPorModo(
        ultimoDisplayMode, ultimaTemperatura, ultimaUmidade,
        ultimaUmidadeSolo, ultimoModoAutomatico, ultimoRelayStatus);
    }
  }

  static void controleLuzDisplay() {
    if (buttonControl.displayBacklight && (millis() - buttonControl.lastButtonTime > Config::DISPLAY_TIMEOUT)) {
      lcd.noBacklight();
      buttonControl.displayBacklight = false;
    }
  }

private:
  static void verificarBotaoMode() {
    if (digitalRead(Config::BTN_MODE) == LOW) {
      if (!buttonControl.modoButtonPressed) {
        buttonControl.modoButtonPressed = true;
        buttonControl.modoButtonStart = millis();
      }

      if ((millis() - buttonControl.modoButtonStart > Config::LONG_PRESS_MODE) && buttonControl.modoButtonPressed) {
        systemState.modoAutomatico = !systemState.modoAutomatico;
        preferences.putBool("modoAuto", systemState.modoAutomatico);
        DisplayManager::mostrarMensagemTemporaria(
          systemState.modoAutomatico ? "MODO AUTO" : "MODO MANUAL");
        buttonControl.modoButtonPressed = false;
        while (digitalRead(Config::BTN_MODE) == LOW)
          ;
      }

      if (!buttonControl.displayBacklight) {
        lcd.backlight();
        buttonControl.displayBacklight = true;
      }
      buttonControl.lastButtonTime = millis();
    } else if (buttonControl.modoButtonPressed) {
      if (millis() - buttonControl.modoButtonStart < Config::LONG_PRESS_MODE) {
        if (systemState.modoAutomatico) {
          DisplayManager::mostrarMensagemTemporaria("TRAVA");
        } else {
          IrrigationController::controleRele(!systemState.relayStatus);
        }
      }
      buttonControl.modoButtonPressed = false;
    }
  }

  static void verificarBotaoDisplay() {
    if (digitalRead(Config::BTN_DISPLAY) == LOW) {
      if (!buttonControl.displayButtonPressed) {
        buttonControl.displayButtonPressed = true;
        buttonControl.displayButtonStart = millis();
      }

      if ((millis() - buttonControl.displayButtonStart > Config::LONG_PRESS_DISPLAY) && buttonControl.displayButtonPressed) {
        DisplayManager::mostrarMensagemTemporaria("Config Avancada");
        WiFiSetup::reset();  // Alterado de WiFiManager::resetWiFi para WiFiSetup::reset
        buttonControl.displayButtonPressed = false;
        while (digitalRead(Config::BTN_DISPLAY) == LOW)
          ;
      }

      if (!buttonControl.displayBacklight) {
        lcd.backlight();
        buttonControl.displayBacklight = true;
      }
      buttonControl.lastButtonTime = millis();
    } else if (buttonControl.displayButtonPressed) {
      if (millis() - buttonControl.displayButtonStart < Config::LONG_PRESS_DISPLAY) {
        systemState.displayMode = (systemState.displayMode + 1) % 5;
      }
      buttonControl.displayButtonPressed = false;
    }
  }

  static bool verificarNecessidadeAtualizacao(
    int& ultimoDisplayMode, float& ultimaTemperatura, float& ultimaUmidade,
    float& ultimaUmidadeSolo, bool& ultimoModoAutomatico, bool& ultimoRelayStatus) {

    return systemState.displayMode != ultimoDisplayMode || (systemState.displayMode == 0 && (systemState.temperatura != ultimaTemperatura || systemState.umidade != ultimaUmidade)) || (systemState.displayMode == 1 && (systemState.umidadeSolo != ultimaUmidadeSolo || systemState.modoAutomatico != ultimoModoAutomatico)) || (systemState.displayMode == 2 && systemState.relayStatus != ultimoRelayStatus);
  }

  static void atualizarDisplayPorModo(
    int& ultimoDisplayMode, float& ultimaTemperatura, float& ultimaUmidade,
    float& ultimaUmidadeSolo, bool& ultimoModoAutomatico, bool& ultimoRelayStatus) {

    lcd.clear();

    switch (systemState.displayMode) {
      case 0:
        atualizarDisplayModo0(ultimaTemperatura, ultimaUmidade);
        break;
      case 1:
        atualizarDisplayModo1(ultimaUmidadeSolo, ultimoModoAutomatico);
        break;
      case 2:
        atualizarDisplayModo2(ultimoRelayStatus);
        break;
      case 3:
        DisplayManager::mostrarInfoWiFi();
        break;
      case 4:
        atualizarDisplayModo4();
        break;
    }

    ultimoDisplayMode = systemState.displayMode;
  }

  static void atualizarDisplayModo0(float& ultimaTemperatura, float& ultimaUmidade) {
    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(systemState.temperatura, 1);
    lcd.print("C  ");

    lcd.setCursor(8, 0);
    lcd.print("U:");
    lcd.print(systemState.umidade, 1);
    lcd.print("%");

    lcd.setCursor(0, 1);
    lcd.print("Hora:");
    char horaLocal[20];
    snprintf(horaLocal, sizeof(horaLocal), "%02d:%02d:%02d",
             agendamento.horaAtual,
             agendamento.minutoAtual,
             timeClient.getSeconds());
    lcd.print(horaLocal);

    ultimaTemperatura = systemState.temperatura;
    ultimaUmidade = systemState.umidade;
  }

  static void atualizarDisplayModo1(float& ultimaUmidadeSolo, bool& ultimoModoAutomatico) {
    lcd.setCursor(0, 0);
    lcd.print("Solo: ");
    lcd.print(systemState.umidadeSolo, 1);
    lcd.print("%");

    lcd.setCursor(0, 1);
    lcd.print(systemState.modoAutomatico ? "Modo: Auto" : "Modo: Manual");

    ultimaUmidadeSolo = systemState.umidadeSolo;
    ultimoModoAutomatico = systemState.modoAutomatico;
  }

  static void atualizarDisplayModo2(bool& ultimoRelayStatus) {
    lcd.setCursor(0, 0);
    lcd.print("Rele:");

    lcd.setCursor(0, 1);
    lcd.print(systemState.relayStatus ? "Ligado" : "Desligado");

    ultimoRelayStatus = systemState.relayStatus;
  }

  static void atualizarDisplayModo4() {
    lcd.setCursor(0, 0);
    char horaFormatada[6];
    snprintf(horaFormatada, sizeof(horaFormatada), "%02d:%02d",
             agendamento.horaLigar, agendamento.minutoLigar);
    lcd.print("Prog: ");
    lcd.print(horaFormatada);

    lcd.setCursor(0, 1);
    String diasSelecionados = "";
    for (int i = 0; i < 7; i++) {
      // Corrigido o operador ternário para usar String ao invés de char
      diasSelecionados += agendamento.diasSemana[i] ? String(diasNomes[i][0]) : String("-");
    }
    lcd.print(diasSelecionados);
  }
};

void setup() {
  pinMode(Config::RELAY_PIN, OUTPUT);
  pinMode(Config::BTN_MODE, INPUT_PULLUP);
  pinMode(Config::BTN_DISPLAY, INPUT_PULLUP);

  preferences.begin("irrigacao", false);
  systemState.modoAutomatico = preferences.getBool("modoAuto", true);
  systemState.relayStatus = preferences.getBool("relay", false);
  digitalWrite(Config::RELAY_PIN, systemState.relayStatus);

  lcd.init();
  lcd.backlight();
  buttonControl.displayBacklight = true;

  dht.begin();
  Serial.begin(115200);

  WiFiSetup::setup();
  timeClient.begin();
  timeClient.update();

  ScheduleManager::carregarConfigAgendamento();

  sensorTicker.attach(3, SensorManager::lerSensores);
}

void loop() {
  wm.process();
  InterfaceManager::verificarBotoes();
  InterfaceManager::controleLuzDisplay();

  static unsigned long ultimaAtualizacaoTempo = 0;
  if (millis() - ultimaAtualizacaoTempo > 1000) {
    ScheduleManager::atualizarDataHora();
    ultimaAtualizacaoTempo = millis();
  }

  IrrigationController::verificarIrrigacaoAutomatica();

  if (!buttonControl.showingMessage || (millis() - buttonControl.messageStartTime > Config::TEMP_MESSAGE_DURATION)) {
    InterfaceManager::atualizarDisplay();
  }

  delay(100);
}