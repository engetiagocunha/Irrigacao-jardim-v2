
#include <ESPmDNS.h>      // Biblioteca para mDNS
#include <WiFiManager.h>  // Biblioteca para WiFiManager
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <Preferences.h>
#include <Ticker.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define DHTPIN 13
#define NUM_SOIL_SENSORS 2
#define SOIL_SENSOR1 12
#define SOIL_SENSOR2 14
#define RELAY_PIN 15
#define BTN_MODE 27
#define BTN_DISPLAY 26

#define DISPLAY_TIMEOUT 60000       // 1 minuto
#define LONG_PRESS_MODE 2000        // 2 segundos
#define LONG_PRESS_DISPLAY 5000     // 5 segundos
#define TEMP_MESSAGE_DURATION 1500  // 1.5 segundos


WiFiManager wm;  // Instância global do WiFiManager

LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHTPIN, DHT11);
Preferences preferences;
Ticker sensorTicker;

// Novas inclusões para NTP e agendamento
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -10800, 60000);

bool agendamentoJaExecutado = false;
// Variáveis globais adicionais para controle de tempo de irrigação
unsigned long tempoIrrigacaoInicio = 0;
const unsigned long DURACAO_MAXIMA_IRRIGACAO = 20 * 60 * 1000;  // 20 minutos em milissegundos

// Estrutura para configurações de agendamento
struct AgendamentoConfig {
  int horaAtual = 0;
  int minutoAtual = 0;
  int diaSemanaAtual = 0;
  int horaLigar = 0;
  int minutoLigar = 0;
  bool diasSemana[7] = { true };  // Seg, Ter, Qua, Qui, Sex, Sab, Dom
} agendamento;

const char* diasNomes[7] = { "Seg", "Ter", "Qua", "Qui", "Sex", "Sab", "Dom" };


// Estado consolidado do sistema
struct SystemState {
  bool agendamentoAtivo = false;
  bool modoAutomatico = true;
  bool relayStatus = false;
  int displayMode = 0;
  float temperatura = 0;
  float umidade = 0;
  float umidadeSolo = 0;
} systemState;

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
} buttonControl;

// Funções de suporte
void mostrarMensagemTemporaria(const char* mensagem) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(mensagem);
  buttonControl.showingMessage = true;
  buttonControl.messageStartTime = millis();
}

void controleLuzDisplay() {
  if (buttonControl.displayBacklight && (millis() - buttonControl.lastButtonTime > DISPLAY_TIMEOUT)) {
    lcd.noBacklight();
    buttonControl.displayBacklight = false;
  }
}

void salvarEstado() {
  preferences.putBool("modoAuto", systemState.modoAutomatico);
  preferences.putBool("relay", systemState.relayStatus);
}

void mostrarInfoWiFi() {
  // Calcula a força do sinal em porcentagem
  int rssi = WiFi.RSSI();
  int signalStrength;

  // Conversão de RSSI para porcentagem
  if (rssi <= -100) {
    signalStrength = 0;
  } else if (rssi >= -50) {
    signalStrength = 100;
  } else {
    signalStrength = 2 * (rssi + 100);
  }

  // Cria caracteres personalizados para blocos de sinal
  byte signalBlocks[5][8] = {
    { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1F },        // 1 bloco
    { 0x0, 0x0, 0x0, 0x0, 0x0, 0x1F, 0x1F, 0x1F },      // 2 blocos
    { 0x0, 0x0, 0x0, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F },    // 3 blocos
    { 0x0, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F },  // 4 blocos
    { 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F }  // 5 blocos (cheio)
  };


  // Registra os caracteres personalizados
  for (int i = 0; i < 5; i++) {
    lcd.createChar(i, signalBlocks[i]);
  }

  // Mostra informações no LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WIFI:");

  // Desenha blocos de sinal
  int numBlocks = map(signalStrength, 0, 100, 0, 5);
  for (int i = 0; i < numBlocks; i++) {
    lcd.setCursor(6 + i, 0);
    lcd.write(byte(i));
  }

  // Mostra porcentagem
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

// Funções de agendamento
void inicializarNTP() {
  timeClient.begin();
  timeClient.update();
}

void salvarConfigAgendamento() {
  preferences.begin("agendamento", false);
  preferences.putInt("hora", agendamento.horaLigar);
  preferences.putInt("minuto", agendamento.minutoLigar);

  // Salva dias da semana como inteiro de bits
  uint8_t diasSalvar = 0;
  for (int i = 0; i < 7; i++) {
    if (agendamento.diasSemana[i]) {
      diasSalvar |= (1 << i);
    }
  }
  preferences.putUChar("diasSemana", diasSalvar);

  preferences.end();
}

void carregarConfigAgendamento() {
  preferences.begin("agendamento", true);
  agendamento.horaLigar = preferences.getInt("hora", 0);
  agendamento.minutoLigar = preferences.getInt("minuto", 0);

  uint8_t diasCarregados = preferences.getUChar("diasSemana", 0);
  for (int i = 0; i < 7; i++) {
    agendamento.diasSemana[i] = (diasCarregados & (1 << i)) != 0;
  }

  preferences.end();
}

bool verificarAgendamento() {
  timeClient.update();
  agendamento.horaAtual = timeClient.getHours();
  agendamento.minutoAtual = timeClient.getMinutes();
  agendamento.diaSemanaAtual = timeClient.getDay();

  // Verifica se o dia atual está nos dias programados
  bool diaCorreto = agendamento.diasSemana[agendamento.diaSemanaAtual];
  bool horarioCorreto = (agendamento.horaAtual == agendamento.horaLigar && agendamento.minutoAtual == agendamento.minutoLigar);

  return diaCorreto && horarioCorreto;
}

void atualizarDataHora() {
  timeClient.update();

  // Atualiza variáveis globais de hora e minuto
  agendamento.horaAtual = timeClient.getHours();
  agendamento.minutoAtual = timeClient.getMinutes();
  agendamento.diaSemanaAtual = timeClient.getDay();
}


// Função para configurar o WiFiManager
void setupWiFi() {
  WiFi.mode(WIFI_STA);
  Serial.println("\nIniciando configuração do WiFi...");

  bool wm_nonblocking = true;
  if (wm_nonblocking) wm.setConfigPortalBlocking(false);

  // Menu customizado com nova opção
  std::vector<const char*> menu = { "wifi", "info", "param", "exit" };
  wm.setMenu(menu);
  wm.setClass("invert");
  wm.setConfigPortalTimeout(30);

  // Parâmetros customizados para agendamento
  WiFiManagerParameter custom_hora("hora", "Hora para Ligar",
                                   String(agendamento.horaLigar).c_str(), 2);
  WiFiManagerParameter custom_minuto("minuto", "Minuto para Ligar",
                                     String(agendamento.minutoLigar).c_str(), 2);

  wm.addParameter(&custom_hora);
  wm.addParameter(&custom_minuto);

  // Checkbox para dias da semana
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

  // Conectar automaticamente
  bool res = wm.autoConnect("ESP3201-CONFIG", "12345678");
  if (!res) {
    Serial.println("Falha na conexão ou tempo limite esgotado");
    return;
  }

  Serial.println("Conectado com sucesso!");

  // Verifica se entrou no portal de configuração
  if (wm.getConfigPortalActive()) {
    // Salva parâmetros
    agendamento.horaLigar = atoi(custom_hora.getValue());
    agendamento.minutoLigar = atoi(custom_minuto.getValue());

    // Salva dias da semana
    for (int i = 0; i < 7; i++) {
      char paramName[10];
      snprintf(paramName, sizeof(paramName), "dia%d", i);
      agendamento.diasSemana[i] = (wm.server->hasArg(paramName) && wm.server->arg(paramName) == "1");
    }

    salvarConfigAgendamento();
  }

  // Inicializa NTP após conexão
  inicializarNTP();
}

// Função para resetar o wifi
// Função de reset de WiFi melhorada
void resetWiFi() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Resetando WiFi ...");

  wm.resetSettings();  // Limpa configurações salvas

  // Reinicia o dispositivo
  delay(1000);
  ESP.restart();
}

// Função para leitura de sensores de temperatura e umidade do ambiente e solo
void lerSensores() {
  systemState.temperatura = dht.readTemperature();
  systemState.umidade = dht.readHumidity();

  float soloRawTotal = 0;
  int sensoresValidos = 0;
  const int SOIL_SENSOR_PINS[NUM_SOIL_SENSORS] = { SOIL_SENSOR1, SOIL_SENSOR2 };

  for (int i = 0; i < NUM_SOIL_SENSORS; i++) {
    int soloRaw = analogRead(SOIL_SENSOR_PINS[i]);
    if (soloRaw >= 0 && soloRaw <= 4095) {
      float soloPercentage = map(soloRaw, 4095, 0, 0, 100);
      soloRawTotal += soloPercentage;
      sensoresValidos++;
    }
  }

  systemState.umidadeSolo = (sensoresValidos > 0) ? (soloRawTotal / sensoresValidos) : 0;
}

void controleRele(bool estado) {
  digitalWrite(RELAY_PIN, estado);
  systemState.relayStatus = estado;
  salvarEstado();
}

void verificarBotoes() {
  // Botão MODE
  if (digitalRead(BTN_MODE) == LOW) {
    if (!buttonControl.modoButtonPressed) {
      buttonControl.modoButtonPressed = true;
      buttonControl.modoButtonStart = millis();
    }

    // Pressionamento longo
    if ((millis() - buttonControl.modoButtonStart > LONG_PRESS_MODE) && buttonControl.modoButtonPressed) {
      systemState.modoAutomatico = !systemState.modoAutomatico;
      salvarEstado();
      mostrarMensagemTemporaria(systemState.modoAutomatico ? "MODO AUTO" : "MODO MANUAL");
      buttonControl.modoButtonPressed = false;
      while (digitalRead(BTN_MODE) == LOW)
        ;
    }

    // Ativa luz de fundo
    if (!buttonControl.displayBacklight) {
      lcd.backlight();
      buttonControl.displayBacklight = true;
    }
    buttonControl.lastButtonTime = millis();
  } else if (buttonControl.modoButtonPressed) {
    // Pressionamento curto
    if (millis() - buttonControl.modoButtonStart < LONG_PRESS_MODE) {
      if (systemState.modoAutomatico) {
        mostrarMensagemTemporaria("TRAVA");
      } else {
        controleRele(!systemState.relayStatus);
      }
    }
    buttonControl.modoButtonPressed = false;
  }

  // Botão DISPLAY
  if (digitalRead(BTN_DISPLAY) == LOW) {
    if (!buttonControl.displayButtonPressed) {
      buttonControl.displayButtonPressed = true;
      buttonControl.displayButtonStart = millis();
    }

    // Pressionamento longo
    if ((millis() - buttonControl.displayButtonStart > LONG_PRESS_DISPLAY) && buttonControl.displayButtonPressed) {
      mostrarMensagemTemporaria("Config Avancada");
      resetWiFi();  // Chama resetWiFi com confirmação
      buttonControl.displayButtonPressed = false;
      while (digitalRead(BTN_DISPLAY) == LOW)
        ;
    }

    // Ativa luz de fundo
    if (!buttonControl.displayBacklight) {
      lcd.backlight();
      buttonControl.displayBacklight = true;
    }
    buttonControl.lastButtonTime = millis();
  } else if (buttonControl.displayButtonPressed) {
    // Pressionamento curto
    if (millis() - buttonControl.displayButtonStart < LONG_PRESS_DISPLAY) {
      systemState.displayMode = (systemState.displayMode + 1) % 5;
    }
    buttonControl.displayButtonPressed = false;
  }
}

void atualizarDisplay() {
  // Variáveis estáticas para comparação
  static int ultimoDisplayMode = -1;
  static float ultimaTemperatura = -1;
  static float ultimaUmidade = -1;
  static float ultimaUmidadeSolo = -1;
  static bool ultimoModoAutomatico = false;
  static bool ultimoRelayStatus = false;
  static int ultimoSegundo = -1;

  // Verifica se há mensagem temporária ativa
  if (buttonControl.showingMessage && (millis() - buttonControl.messageStartTime < TEMP_MESSAGE_DURATION)) {
    return;
  }

  buttonControl.showingMessage = false;

  // Atualiza o cliente NTP para manter o tempo sincronizado
  timeClient.update();

  // Obtém o segundo atual
  int segundoAtual = timeClient.getSeconds();

  // Se mudou o modo de display, limpa a tela completamente
  if (systemState.displayMode != ultimoDisplayMode) {
    lcd.clear();
  }

  // Verifica se precisa atualizar o display
  bool precisaAtualizar =
    systemState.displayMode != ultimoDisplayMode || segundoAtual != ultimoSegundo ||  // Adiciona verificação de segundos
    (systemState.displayMode == 0 && (systemState.temperatura != ultimaTemperatura || systemState.umidade != ultimaUmidade)) || (systemState.displayMode == 1 && (systemState.umidadeSolo != ultimaUmidadeSolo || systemState.modoAutomatico != ultimoModoAutomatico)) || (systemState.displayMode == 2 && systemState.relayStatus != ultimoRelayStatus);

  if (precisaAtualizar) {
    switch (systemState.displayMode) {
      case 0:  // Temperatura, Umidade e Hora
        // Atualiza apenas a linha do tempo se só o tempo mudou
        if (segundoAtual != ultimoSegundo) {
          lcd.setCursor(0, 1);
          lcd.print("Hora:");
          char horaLocal[20];
          snprintf(horaLocal, sizeof(horaLocal), "%02d:%02d:%02d",
                   agendamento.horaAtual,
                   agendamento.minutoAtual,
                   segundoAtual);
          lcd.print(horaLocal);
        }

        // Se outros valores mudaram, atualiza a primeira linha
        if (systemState.temperatura != ultimaTemperatura || systemState.umidade != ultimaUmidade || systemState.displayMode != ultimoDisplayMode) {
          lcd.setCursor(0, 0);
          lcd.print("T:");
          lcd.print(systemState.temperatura, 1);
          lcd.print("C  ");

          lcd.setCursor(8, 0);
          lcd.print("U:");
          lcd.print(systemState.umidade, 1);
          lcd.print("%");
        }
        break;

      case 1:  // Umidade do Solo e Modo de Operação
        lcd.setCursor(0, 0);
        lcd.print("Solo: ");
        lcd.print(systemState.umidadeSolo, 1);
        lcd.print("%");

        lcd.setCursor(0, 1);
        lcd.print(systemState.modoAutomatico ? "Modo: Auto" : "Modo: Manual");

        ultimaUmidadeSolo = systemState.umidadeSolo;
        ultimoModoAutomatico = systemState.modoAutomatico;
        break;

      case 2:  // Estado do Relé
        lcd.setCursor(0, 0);
        lcd.print("Rele:");

        lcd.setCursor(0, 1);
        lcd.print(systemState.relayStatus ? "Ligado" : "Desligado");

        ultimoRelayStatus = systemState.relayStatus;
        break;

      case 3:  // Placeholder para Informações de Rede
        mostrarInfoWiFi();
        break;

      case 4:  // Informações de Agendamento
        lcd.setCursor(0, 0);
        // Formata hora com zero à esquerda se necessário
        char horaFormatada[6];
        snprintf(horaFormatada, sizeof(horaFormatada), "%02d:%02d",
                 agendamento.horaLigar, agendamento.minutoLigar);
        lcd.print("Prog: ");
        lcd.print(horaFormatada);

        lcd.setCursor(0, 1);
        // Mostra dias selecionados
        String diasSelecionados = "";
        for (int i = 0; i < 7; i++) {
          if (agendamento.diasSemana[i]) {
            diasSelecionados += diasNomes[i][0];  // Primeira letra do dia
          } else {
            diasSelecionados += "-";  // Ou espaço, se preferir
          }
        }
        lcd.print(diasSelecionados);
        break;
    }

    // Atualiza as variáveis de controle
    ultimoSegundo = segundoAtual;
    ultimoDisplayMode = systemState.displayMode;
    ultimaTemperatura = systemState.temperatura;
    ultimaUmidade = systemState.umidade;
    ultimaUmidadeSolo = systemState.umidadeSolo;
    ultimoModoAutomatico = systemState.modoAutomatico;
    ultimoRelayStatus = systemState.relayStatus;
  }
}

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BTN_MODE, INPUT_PULLUP);
  pinMode(BTN_DISPLAY, INPUT_PULLUP);

  preferences.begin("irrigacao", false);
  systemState.modoAutomatico = preferences.getBool("modoAuto", true);
  systemState.relayStatus = preferences.getBool("relay", false);
  digitalWrite(RELAY_PIN, systemState.relayStatus);

  lcd.init();
  lcd.backlight();

  buttonControl.displayBacklight = true;

  dht.begin();
  Serial.begin(115200);

  setupWiFi();

  timeClient.begin();
  timeClient.update();

  // Carregar configuração de agendamento
  carregarConfigAgendamento();

  sensorTicker.attach(3, lerSensores);
}

void loop() {
  wm.process();
  verificarBotoes();
  controleLuzDisplay();

  static unsigned long ultimaAtualizacaoTempo = 0;
  unsigned long tempoAtual = millis();

  // Atualiza o tempo a cada 100ms em vez de 1 segundo
  if (tempoAtual - ultimaAtualizacaoTempo >= 100) {
    atualizarDataHora();
    atualizarDisplay();
    ultimaAtualizacaoTempo = tempoAtual;
  }

  // Lógica de irrigação com agendamento
  if (systemState.modoAutomatico) {

    // Verifica se está na hora e dia programados
    bool horarioAgendado = verificarAgendamento();

    // Condições para ligar a irrigação:
    // 1. Umidade do solo abaixo de 40%
    // 2. Está no horário e dia agendados
    // 3. Relé não está ligado
    if (systemState.umidadeSolo < 40 && horarioAgendado && !systemState.relayStatus) {
      controleRele(true);
      tempoIrrigacaoInicio = millis();  // Marca o início da irrigação
    }

    // Condições para desligar a irrigação:
    // 1. Umidade do solo atingiu 100%
    // 2. Ou passou 20 minutos desde o início
    if (systemState.relayStatus) {
      if (systemState.umidadeSolo >= 100 || (millis() - tempoIrrigacaoInicio >= DURACAO_MAXIMA_IRRIGACAO)) {
        controleRele(false);
      }
    }
  }


  /*// Lógica de irrigação automática
  if (systemState.modoAutomatico) {
    if (systemState.umidadeSolo < 45 && !systemState.relayStatus) {
      controleRele(true);
    } else if (systemState.umidadeSolo >= 70 && systemState.relayStatus) {
      controleRele(false);
    }
  }*/

  // Atualização de display
  if (!buttonControl.showingMessage || (millis() - buttonControl.messageStartTime > TEMP_MESSAGE_DURATION)) {
    atualizarDisplay();
  }

  yield();  // Permite que outras tarefas do sistema sejam executadas
}