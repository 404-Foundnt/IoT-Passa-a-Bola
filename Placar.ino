// ==================== INCLUSÃO DE BIBLIOTECAS ====================
#include <Wire.h>               // Comunicação I2C
#include <Adafruit_GFX.h>       // Biblioteca gráfica genérica
#include <Adafruit_SSD1306.h>   // Biblioteca específica para displays SSD1306
#include <EEPROM.h>             // Memória EEPROM (armazenamento não-volátil)
#include <WiFi.h>               // Conexão Wi-Fi
#include <PubSubClient.h>       // Cliente MQTT

// ==================== CONFIGURAÇÕES OLED ====================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define I2C_ADDRESS   0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ==================== BOTÕES ====================
// Enum para indexar os botões
enum Button {
  BTN_LEFT, BTN_RIGHT, BTN_UP, BTN_DOWN, BTN_CONFIRM, BTN_BACK, BTN_COUNT
};
// Mapeamento dos botões para os pinos físicos
const int buttonPins[BTN_COUNT] = {32, 33, 25, 26, 27, 14};

// ==================== EEPROM ====================
// Espaço de memória reservado
const int EEPROM_SIZE = 512;

// Estrutura para armazenar placar e nomes dos times/jogadores
struct ScoreData {
  uint16_t teamScore[2];         // Placar dos times
  uint16_t playerPoints[2][11];  // Pontos individuais (até 11 jogadores por time)
  char teamName[2][16];          // Nome dos times
};
ScoreData dataStorage;

// ==================== WIFI + MQTT ====================
const char* SSID        = "Wokwi-GUEST";
const char* PASSWORD    = "";
const char* BROKER_MQTT = "20.151.77.156";
const int   BROKER_PORT = 1883;
const char* TOPICO_CMD  = "/TEF/placar001/cmd"; // tópico de comando
const char* ID_MQTT     = "esp32_placar";

WiFiClient espClient;
PubSubClient MQTT(espClient);
unsigned long lastReconnectAttempt = 0; // controle de reconexão

char msgBuffer[256];  // Buffer para mensagens MQTT
char topicBuffer[64]; // Buffer para tópicos MQTT

// ==================== INTERFACE ====================
// Controle de telas
enum ScreenState { SCREEN_MAIN, SCREEN_PLAYER_SELECT };
ScreenState screen = SCREEN_MAIN;

// Controle de seleção atual
int selectedTeam   = 0;
int selectedPlayer = 0;
bool playerModeAdd = true; // true = adicionar pontos, false = remover

// Variáveis auxiliares para detectar mudanças e evitar redesenho desnecessário
int lastSelectedTeam = -1;
int lastSelectedPlayer = -1;

// ==================== FUNÇÕES EEPROM ====================
void saveData() {
  EEPROM.put(0, dataStorage); // Salva struct na EEPROM
  EEPROM.commit();
}

void loadData() {
  EEPROM.get(0, dataStorage); // Carrega dados da EEPROM
  // Se não estiver inicializado, define valores padrão
  if (dataStorage.teamName[0][0] == '\0' || dataStorage.teamName[0][0] == 0xFF) {
    Serial.println("EEPROM nao inicializada. Configurando valores padrao...");
    strcpy(dataStorage.teamName[0], "Time A");
    strcpy(dataStorage.teamName[1], "Time B");
    resetData(); // Zera placares
  }
}

void resetData() {
  // Zera todos os pontos
  for (int t = 0; t < 2; t++) {
    dataStorage.teamScore[t] = 0;
    for (int p = 0; p < 11; p++) dataStorage.playerPoints[t][p] = 0;
  }
  saveData();
}

// ==================== PUBLICAÇÃO DO PLACAR ====================
void publishScores() {
  char topicBuffer[64];
  char msgBuffer[256];
  snprintf(topicBuffer, sizeof(topicBuffer), "/%s/%s/attrs", "TEF", "placar001");

  Serial.printf("Preparando para publicar no tópico: %s\n", topicBuffer);

  snprintf(msgBuffer, sizeof(msgBuffer), "TA|%d", dataStorage.teamScore[0]);
  MQTT.publish(topicBuffer, msgBuffer);
  Serial.printf("PUBLISH -> Mensagem: %s\n", msgBuffer);

  snprintf(msgBuffer, sizeof(msgBuffer), "TB|%d", dataStorage.teamScore[1]);
  MQTT.publish(topicBuffer, msgBuffer);
  Serial.printf("PUBLISH -> Mensagem: %s\n", msgBuffer);

  char player_buffer[150];
  player_buffer[0] = '\0';
  char temp_buffer[80];
  snprintf(temp_buffer, sizeof(temp_buffer), "A:%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
    dataStorage.playerPoints[0][0], dataStorage.playerPoints[0][1], dataStorage.playerPoints[0][2],
    dataStorage.playerPoints[0][3], dataStorage.playerPoints[0][4], dataStorage.playerPoints[0][5],
    dataStorage.playerPoints[0][6], dataStorage.playerPoints[0][7], dataStorage.playerPoints[0][8],
    dataStorage.playerPoints[0][9], dataStorage.playerPoints[0][10]);
  strcat(player_buffer, temp_buffer); 
  snprintf(temp_buffer, sizeof(temp_buffer), "|B:%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
    dataStorage.playerPoints[1][0], dataStorage.playerPoints[1][1], dataStorage.playerPoints[1][2],
    dataStorage.playerPoints[1][3], dataStorage.playerPoints[1][4], dataStorage.playerPoints[1][5],
    dataStorage.playerPoints[1][6], dataStorage.playerPoints[1][7], dataStorage.playerPoints[1][8],
    dataStorage.playerPoints[1][9], dataStorage.playerPoints[1][10]);
  strcat(player_buffer, temp_buffer); 
  snprintf(msgBuffer, sizeof(msgBuffer), "j|%s", player_buffer);
  MQTT.publish(topicBuffer, msgBuffer);
  Serial.printf("PUBLISH -> Mensagem: %s\n", msgBuffer);
}

// Função para receber os comandos
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  if (length >= sizeof(msgBuffer)) return;
  memcpy(msgBuffer, payload, length);
  msgBuffer[length] = '\0';

  Serial.print("Comando recebido: ");
  Serial.println(msgBuffer);

  char* command_start = strchr(msgBuffer, '@');
  if (command_start == NULL) return;
  command_start++;

  if (strncmp(command_start, "reset", 5) == 0) {
    Serial.println("Comando RESET executado!");
    resetData();
    publishScores();
  }

  if (strncmp(command_start, "addPoint", 8) == 0 || strncmp(command_start, "removePoint", 11) == 0) {
    char* value_start = strchr(command_start, '|');
    if (value_start == NULL) return;
    value_start++;

    int time = -1, jogador = -1;
    char* time_ptr = strstr(value_start, "time=");
    char* jogador_ptr = strstr(value_start, "jogador=");

    if (time_ptr != NULL && jogador_ptr != NULL) {
      sscanf(time_ptr + 5, "%d", &time);
      sscanf(jogador_ptr + 8, "%d", &jogador);
      
      if (time >= 0 && time <= 1 && jogador >= 0 && jogador <= 10) {
        selectedTeam = time;
        selectedPlayer = jogador;
        
        if (strncmp(command_start, "addPoint", 8) == 0) {
          playerModeAdd = true;
          Serial.printf("Comando ADDPOINT executado! Time: %d, Jogador: %d\n", time, jogador);
        } else {
          playerModeAdd = false;
          Serial.printf("Comando REMOVEPOINT executado! Time: %d, Jogador: %d\n", time, jogador);
        }
        applyPointChange();
      }
    }
  }
}

// ==================== CONEXÕES WIFI + MQTT ====================
void checkConnections() {
  // Reconexão Wi-Fi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi desconectado. Tentando reconectar...");
    WiFi.begin(SSID, PASSWORD);
  }
  // Reconexão MQTT
  if (!MQTT.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      Serial.print("Tentando conectar ao broker MQTT...");
      if (MQTT.connect(ID_MQTT)) {
        Serial.println(" conectado!");
        MQTT.subscribe(TOPICO_CMD); // inscreve no tópico de comando
      } else {
        Serial.print(" falhou, estado: ");
        Serial.println(MQTT.state());
      }
    }
  }
}

// ==================== VARIÁVEIS PARA REDESENHO OLED ====================
ScreenState lastScreen = SCREEN_MAIN;
int lastScore[2] = {-1, -1};
int lastPlayerPoints = -1;
bool lastMode = true;

// ==================== TELA PRINCIPAL ====================
void drawMainScreen() {
  // Só redesenha se houver mudança
  if (lastScreen == SCREEN_MAIN &&
      lastScore[0] == dataStorage.teamScore[0] &&
      lastScore[1] == dataStorage.teamScore[1] &&
      lastSelectedTeam == selectedTeam) return;

  display.clearDisplay();
  display.setTextWrap(false);

  display.setTextSize(2);
  display.setCursor(15, 0);  
  display.print(dataStorage.teamName[0]);
  display.setCursor(15, 16);
  display.print(dataStorage.teamScore[0]);

  display.setCursor(15, 32); 
  display.print(dataStorage.teamName[1]);
  display.setCursor(15, 48); 
  display.print(dataStorage.teamScore[1]);

  // Mostra seta de seleção
  display.setTextSize(3); 
  display.setCursor(0, selectedTeam == 0 ? 8 : 40);
  display.print(">");
  display.display();

  // Atualiza estado
  lastScreen = SCREEN_MAIN;
  lastScore[0] = dataStorage.teamScore[0];
  lastScore[1] = dataStorage.teamScore[1];
  lastSelectedTeam = selectedTeam;
}

// ==================== TELA JOGADOR ====================
void drawPlayerScreen() {
  int currentPoints = dataStorage.playerPoints[selectedTeam][selectedPlayer];
  // Só redesenha se houve mudança
  if (lastScreen == SCREEN_PLAYER_SELECT &&
      lastPlayerPoints == currentPoints &&
      lastMode == playerModeAdd &&
      lastSelectedPlayer == selectedPlayer) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(dataStorage.teamName[selectedTeam]);
  display.setCursor(0, 12);
  display.print("Jogador ");
  display.print(selectedPlayer + 1);
  display.setCursor(0, 24);
  display.print("Pontos: ");
  display.print(currentPoints);
  display.setCursor(0, 40);
  display.print(playerModeAdd ? "[ADICIONAR]" : "[REMOVER]");
  display.display();

  lastScreen = SCREEN_PLAYER_SELECT;
  lastPlayerPoints = currentPoints;
  lastMode = playerModeAdd;
  lastSelectedPlayer = selectedPlayer;
}

// ==================== APLICA PONTO ====================
void applyPointChange() {
  if (playerModeAdd) {
    dataStorage.playerPoints[selectedTeam][selectedPlayer]++;
    dataStorage.teamScore[selectedTeam]++;
  } else if (dataStorage.playerPoints[selectedTeam][selectedPlayer] > 0) {
    dataStorage.playerPoints[selectedTeam][selectedPlayer]--;
    if (dataStorage.teamScore[selectedTeam] > 0) dataStorage.teamScore[selectedTeam]--;
  }
  saveData();
  publishScores();
}

// ==================== BOTÕES ====================
bool buttonPressed(Button btn) {
  return digitalRead(buttonPins[btn]) == LOW; // Botão pressionado = nível baixo
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  loadData();

  // Configura botões
  for (int i = 0; i < BTN_COUNT; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  // Conexão Wi-Fi
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(100);
  Serial.println("Wi-Fi conectado!");

  // Inicializa OLED
  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, I2C_ADDRESS)) {
    Serial.println(F("Falha ao alocar SSD1306"));
    for (;;); // trava se falhar
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Wi-Fi Conectado!");
  display.display();

  // Configura MQTT
  MQTT.setServer(BROKER_MQTT, BROKER_PORT);
  MQTT.setCallback(mqtt_callback);

  drawMainScreen();
  publishScores();
}

// ==================== LOOP ====================
void loop() {
  checkConnections(); // Mantém conexões
  MQTT.loop();        // Processa mensagens MQTT

  static unsigned long lastPress = 0;
  if (millis() - lastPress < 200) return; // debounce simples (200ms)

  // Navegação entre times/jogadores
  if (buttonPressed(BTN_UP)) {
    if (screen == SCREEN_MAIN) selectedTeam = 0;
    else selectedPlayer = (selectedPlayer + 1) % 11;
    lastPress = millis();
  } 
  else if (buttonPressed(BTN_DOWN)) {
    if (screen == SCREEN_MAIN) selectedTeam = 1;
    else selectedPlayer = (selectedPlayer + 10) % 11;
    lastPress = millis();
  }
  else if (buttonPressed(BTN_LEFT) && screen == SCREEN_PLAYER_SELECT) {
    playerModeAdd = true;
    lastPress = millis();
  } 
  else if (buttonPressed(BTN_RIGHT) && screen == SCREEN_PLAYER_SELECT) {
    playerModeAdd = false;
    lastPress = millis();
  } 
  else if (buttonPressed(BTN_CONFIRM)) {
    if (screen == SCREEN_MAIN) screen = SCREEN_PLAYER_SELECT;
    else applyPointChange();
    lastPress = millis();
  } 
  else if (buttonPressed(BTN_BACK) && screen == SCREEN_PLAYER_SELECT) {
    screen = SCREEN_MAIN;
    lastPress = millis();
  }

  // Atualiza a tela de acordo com o estado
  if (screen == SCREEN_MAIN) drawMainScreen();
  else drawPlayerScreen();
}
