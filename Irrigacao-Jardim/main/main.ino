#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <Preferences.h>
#include <Ticker.h>

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

LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHTPIN, DHT11);
Preferences preferences;
Ticker sensorTicker;

// Estado consolidado do sistema
struct SystemState {
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
  // Variáveis estáticas para comparação e redução de redraws
  static int ultimoDisplayMode = -1;
  static float ultimaTemperatura = -1;
  static float ultimaUmidade = -1;
  static float ultimaUmidadeSolo = -1;
  static bool ultimoModoAutomatico = false;
  static bool ultimoRelayStatus = false;

  // Verifica se há mensagem temporária ativa
  if (buttonControl.showingMessage && (millis() - buttonControl.messageStartTime < TEMP_MESSAGE_DURATION)) {
    return;
  }

  buttonControl.showingMessage = false;

  // Verifica se precisa atualizar o display
  bool precisaAtualizar =
    systemState.displayMode != ultimoDisplayMode || (systemState.displayMode == 0 && (systemState.temperatura != ultimaTemperatura || systemState.umidade != ultimaUmidade)) || (systemState.displayMode == 1 && (systemState.umidadeSolo != ultimaUmidadeSolo || systemState.modoAutomatico != ultimoModoAutomatico)) || (systemState.displayMode == 2 && systemState.relayStatus != ultimoRelayStatus);

  if (precisaAtualizar) {
    lcd.clear();

    switch (systemState.displayMode) {
      case 0:  // Temperatura e Umidade
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
        lcd.setCursor(0, 0);
        lcd.print("WIFI:");

        lcd.setCursor(0, 1);
        lcd.print("Conectando...");
        break;

      case 4:  // Placeholder para Informações de Data/Hora
        lcd.setCursor(0, 0);
        lcd.print("Sistema:");

        lcd.setCursor(0, 1);
        lcd.print("Inicializando");
        break;
    }

    // Atualiza o modo de display atual
    ultimoDisplayMode = systemState.displayMode;
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

  sensorTicker.attach(3, lerSensores);
}

void loop() {
  verificarBotoes();
  controleLuzDisplay();

  // Lógica de modo automático
  if (systemState.modoAutomatico) {
    if (systemState.umidadeSolo < 45 && !systemState.relayStatus) {
      controleRele(true);
    } else if (systemState.umidadeSolo >= 70 && systemState.relayStatus) {
      controleRele(false);
    }
  }

  // Atualização de display
  if (!buttonControl.showingMessage || (millis() - buttonControl.messageStartTime > TEMP_MESSAGE_DURATION)) {
    atualizarDisplay();
  }

  delay(100);
}