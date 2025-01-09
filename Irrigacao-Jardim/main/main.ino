/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SEÇÃO 1: BIBLIOTECAS E DEFINIÇÕES
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <Preferences.h>
#include <Ticker.h>

// Definições de Pinos
#define DHTPIN 13
#define NUM_SOIL_SENSORS 2
#define SOIL_SENSOR1 12
#define SOIL_SENSOR2 14
#define RELAY_PIN 15
#define BTN_MODE 27
#define BTN_DISPLAY 26

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SEÇÃO 2: DECLARAÇÕES GLOBAIS
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Objetos de Componentes
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHTPIN, DHT11);
Preferences preferences;
Ticker sensorTicker;

// Variáveis de Estado
struct SystemState {
  bool modoAutomatico = false;
  bool relayStatus = true;
  int displayMode = 0;
  float temperatura = 0;
  float umidade = 0;
  float umidadeSolo = 0;
};
SystemState systemState;

// Variáveis de Controle de Botões
struct ButtonControl {
  unsigned long modoButtonPressStart = 0;
  bool modoButtonPressed = false;
  unsigned long displayButtonPressStart = 0;
  bool displayButtonPressed = false;
  bool mostrandoMensagem = false;
  unsigned long tempoMensagem = 0;
};
ButtonControl buttonControl;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SEÇÃO 3: FUNÇÕES AUXILIARES
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Função para mostrar mensagem temporária
void mostrarMensagemTemporaria(const char* mensagem) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(mensagem);
  buttonControl.mostrandoMensagem = true;
  buttonControl.tempoMensagem = millis();
}

// Função para salvar estado do sistema
void salvarEstado() {
  preferences.putBool("modoAuto", systemState.modoAutomatico);
  preferences.putBool("relay", systemState.relayStatus);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SEÇÃO 4: FUNÇÕES DE SENSOR E CONTROLE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Função para ler sensores
void lerSensores() {
  // Sensor de umidade e temperatura do ambiente
  systemState.temperatura = dht.readTemperature();
  systemState.umidade = dht.readHumidity();

  // Sensor de umidade do solo
  float soloRawTotal = 0;
  int sensoresValidos = 0;
  const int SOIL_SENSOR_PINS[NUM_SOIL_SENSORS] = { SOIL_SENSOR1, SOIL_SENSOR2 };

  for (int i = 0; i < NUM_SOIL_SENSORS; i++) {
    int soloRaw = analogRead(SOIL_SENSOR_PINS[i]);

    // Verificar se a leitura é válida
    if (soloRaw >= 0 && soloRaw <= 4095) {
      float soloPercentage = map(soloRaw, 4095, 0, 0, 100);
      soloRawTotal += soloPercentage;
      sensoresValidos++;
    }
  }

  // Evitar divisão por zero
  systemState.umidadeSolo = (sensoresValidos > 0) ? (soloRawTotal / sensoresValidos) : 0;
}

// Função para controlar o relé
void controleRele(bool estado) {
  digitalWrite(RELAY_PIN, estado);
  systemState.relayStatus = estado;
  salvarEstado();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SEÇÃO 5: FUNÇÕES DE INTERFACE E INTERAÇÃO
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Verificação de botões
void verificarBotoes() {
  // Botão Mode
  if (digitalRead(BTN_MODE) == LOW) {
    if (!buttonControl.modoButtonPressed) {
      buttonControl.modoButtonPressed = true;
      buttonControl.modoButtonPressStart = millis();
    }

    // Pressionamento longo (2 segundos)
    if ((millis() - buttonControl.modoButtonPressStart > 2000) && buttonControl.modoButtonPressed) {
      systemState.modoAutomatico = !systemState.modoAutomatico;
      salvarEstado();
      mostrarMensagemTemporaria(systemState.modoAutomatico ? "MODO AUTO" : "MODO MANUAL");
      buttonControl.modoButtonPressed = false;
      while (digitalRead(BTN_MODE) == LOW)
        ;
    }
  }
  // Soltar o botão
  else if (buttonControl.modoButtonPressed) {
    // Pressionamento curto
    if (millis() - buttonControl.modoButtonPressStart < 2000) {
      if (systemState.modoAutomatico) {
        mostrarMensagemTemporaria("TRAVA");
      } else {
        controleRele(!systemState.relayStatus);
      }
    }
    buttonControl.modoButtonPressed = false;
  }

  // Botão Display
  if (digitalRead(BTN_DISPLAY) == LOW) {
    if (!buttonControl.displayButtonPressed) {
      // Primeiro momento do pressionamento
      buttonControl.displayButtonPressed = true;
      buttonControl.displayButtonPressStart = millis();
    }

    // Verifica se o botão foi mantido pressionado por 5 segundos
    if ((millis() - buttonControl.displayButtonPressStart > 5000) && buttonControl.displayButtonPressed) {
      // Chama função de pressionamento longo
      funcaoPressionamentoLongoDisplay();

      // Reseta o estado do botão
      buttonControl.displayButtonPressed = false;

      // Espera soltar o botão
      while (digitalRead(BTN_DISPLAY) == LOW)
        ;
    }
  } else if (buttonControl.displayButtonPressed) {
    // Botão foi solto
    if (millis() - buttonControl.displayButtonPressStart < 5000) {
      // Pressionamento curto - muda o display
      systemState.displayMode = (systemState.displayMode + 1) % 5;
    }

    // Reseta o estado do botão
    buttonControl.displayButtonPressed = false;
  }
}

// Função de pressionamento longo do display
void funcaoPressionamentoLongoDisplay() {
  // Exemplo de implementação mais elaborada
  mostrarMensagemTemporaria("Config Avancada");
  // Aqui você pode adicionar lógica para entrar em um modo de configuração
}

// Atualizar display
void atualizarDisplay() {
  static int ultimoDisplayMode = -1;
  static float ultimaTemperatura = -1;
  static float ultimaUmidade = -1;
  static float ultimaUmidadeSolo = -1;
  static bool ultimoModoAutomatico = !systemState.modoAutomatico;
  static bool ultimoRelayStatus = !systemState.relayStatus;

  // Verifica se há mensagem temporária ativa
  if (buttonControl.mostrandoMensagem && (millis() - buttonControl.tempoMensagem < 1500)) {
    return;
  }

  buttonControl.mostrandoMensagem = false;

  // Só redesenha se houver mudança
  bool needsUpdate =
    systemState.displayMode != ultimoDisplayMode || (systemState.displayMode == 0 && (systemState.temperatura != ultimaTemperatura || systemState.umidade != ultimaUmidade)) || (systemState.displayMode == 1 && (systemState.umidadeSolo != ultimaUmidadeSolo || systemState.modoAutomatico != ultimoModoAutomatico)) || (systemState.displayMode == 2 && systemState.relayStatus != ultimoRelayStatus);

  if (needsUpdate) {
    lcd.clear();

    switch (systemState.displayMode) {
      case 0:
        lcd.setCursor(0, 0);
        lcd.print("Temp: ");
        lcd.print(systemState.temperatura, 1);
        lcd.print("C");
        lcd.setCursor(0, 1);
        lcd.print("Umid: ");
        lcd.print(systemState.umidade, 1);
        lcd.print("%");
        ultimaTemperatura = systemState.temperatura;
        ultimaUmidade = systemState.umidade;
        break;

      case 1:
        lcd.setCursor(0, 0);
        lcd.print("Umid Solo: ");
        lcd.print(systemState.umidadeSolo, 1);
        lcd.print("%");
        lcd.setCursor(0, 1);
        lcd.print(systemState.modoAutomatico ? "Modo: Auto" : "Modo: Manual");
        ultimaUmidadeSolo = systemState.umidadeSolo;
        ultimoModoAutomatico = systemState.modoAutomatico;
        break;

      case 2:
        lcd.setCursor(0, 0);
        lcd.print("Estado Rele:");
        lcd.setCursor(0, 1);
        lcd.print(systemState.relayStatus ? "Ligado" : "Desligado");
        ultimoRelayStatus = systemState.relayStatus;
        break;

      case 3:
        lcd.setCursor(0, 0);
        lcd.print("WIFI:");
        lcd.setCursor(0, 1);
        lcd.print("IP: Configurando");
        break;

      case 4:
        lcd.setCursor(0, 0);
        lcd.print("DATA:");
        lcd.setCursor(0, 1);
        lcd.print("Hora: Carregando");
        break;
    }

    ultimoDisplayMode = systemState.displayMode;
  }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SEÇÃO 6: CONFIGURAÇÃO E LOOP PRINCIPAL
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  // Configuração de pinos
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BTN_MODE, INPUT_PULLUP);
  pinMode(BTN_DISPLAY, INPUT_PULLUP);

  // Inicialização de preferências
  preferences.begin("irrigacao", false);
  systemState.modoAutomatico = preferences.getBool("modoAuto", true);
  systemState.relayStatus = preferences.getBool("relay", true);
  digitalWrite(RELAY_PIN, systemState.relayStatus);

  // Inicialização de componentes
  lcd.init();
  lcd.backlight();
  dht.begin();
  Serial.begin(115200);

  // Configuração de leitura de sensores
  sensorTicker.attach(3, lerSensores);
}

void loop() {
  verificarBotoes();

  // Lógica de modo automático
  if (systemState.modoAutomatico) {
    if (systemState.umidadeSolo < 45 && !systemState.relayStatus) {
      controleRele(true);
    } else if (systemState.umidadeSolo >= 70 && systemState.relayStatus) {
      controleRele(false);
    }
  }

  // Atualização de display
  if (!buttonControl.mostrandoMensagem || (millis() - buttonControl.tempoMensagem > 1500)) {
    atualizarDisplay();
  }

  delay(100);
}