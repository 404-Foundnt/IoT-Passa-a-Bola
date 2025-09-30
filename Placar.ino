#include <Wire.h>                // Para comunicação I2C com periféricos (display OLED).
#include <Adafruit_GFX.h>        // Biblioteca gráfica base para os displays da Adafruit.
#include <Adafruit_SSD1306.h>    // Driver específico para o display OLED com controlador SSD1306.
#include <EEPROM.h>              // Para ler e escrever na memória não volátil do ESP32.
#include <WiFi.h>                // Para conectar o ESP32 a redes Wi-Fi.
#include <PubSubClient.h>        // Para atuar como um cliente MQTT, comunicando-se com o broker.

// ==================== CONFIGURAÇÃO DO DISPLAY ====================
#define SCREEN_WIDTH 128         // Largura do display em pixels.
#define SCREEN_HEIGHT 64         // Altura do display em pixels.
#define OLED_RESET    -1         // Pino de reset (-1 indica que não há um pino dedicado).
#define I2C_ADDRESS   0x3C       // Endereço I2C do display.
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ==================== BOTÕES ====================
// Enumeração para facilitar a referência aos botões por nome, tornando o código mais legível.
enum Button { BTN_LEFT, BTN_RIGHT, BTN_UP, BTN_DOWN, BTN_CONFIRM, BTN_BACK, BTN_COUNT };
// Array que mapeia cada botão a um pino físico (GPIO) do ESP32.
const int buttonPins[BTN_COUNT] = {32, 33, 25, 26, 27, 14};

// ==================== EEPROM E ESTRUTURAS DE DADOS ====================
const int EEPROM_SIZE = 512;     // Aloca 512 bytes da memória EEPROM para este programa.

// Struct para os dados da PARTIDA ATUAL. Agrupa informações que mudam a cada jogo.
struct ScoreData {
  uint16_t teamScore[2];
  uint16_t playerPoints[2][11];
  char teamName[2][16];
};
ScoreData dataStorage; // Variável global que armazena os dados da partida em andamento.

// Struct para os dados do CAMPEONATO. Agrupa estatísticas que persistem entre os jogos.
struct TeamStats {
  int points = 0;
  int wins = 0;
  int draws = 0;
  int losses = 0;
};
TeamStats championshipData[2]; // Posição 0 para Time A, 1 para Time B.

// ==================== CONFIGURAÇÃO WI-FI E MQTT ====================
const char* SSID        = "Wokwi-GUEST";   // Nome da rede Wi-Fi.
const char* PASSWORD    = "";              // Senha da rede Wi-Fi.
const char* BROKER_MQTT = "20.151.77.156"; // IP do servidor MQTT.
const int   BROKER_PORT = 1883;            // Porta padrão MQTT.
const char* TOPICO_CMD  = "/TEF/placar001/cmd"; // Tópico MQTT que o ESP32 escuta para receber comandos.
const char* ID_MQTT     = "esp32_placar";      // ID único do cliente para o broker.

WiFiClient espClient;
PubSubClient MQTT(espClient);
unsigned long lastReconnectAttempt = 0;

// Buffers para formatar as mensagens e tópicos MQTT de forma segura.
char msgBuffer[256];
char topicBuffer[64];

// ==================== ESTADO DA INTERFACE ====================
// Enumeração para os diferentes estados (telas) da interface.
enum ScreenState { SCREEN_MAIN, SCREEN_PLAYER_SELECT };
ScreenState screen = SCREEN_MAIN; // Variável que armazena a tela atualmente ativa.

int selectedTeam   = 0;
int selectedPlayer = 0;
bool playerModeAdd = true; // Define se a pontuação será adicionada (true) ou removida (false).

// Variáveis para guardar o estado anterior e otimizar o redesenho do display, evitando flicker.
int lastSelectedTeam = -1;
int lastSelectedPlayer = -1;

// Declarações antecipadas das funções para permitir que sejam chamadas em qualquer ordem.
void publishScores();
void saveData();
void saveChampionshipData();
void publishChampionshipData();
void resetData();
void finalizeMatch();
void resetChampionship();

// ==================== FUNÇÕES EEPROM ====================
// Salva a struct 'dataStorage' (partida atual) na EEPROM.
void saveData() {
  EEPROM.put(0, dataStorage);
  EEPROM.commit();
}

// Carrega os dados da partida da EEPROM. Se for a primeira execução, inicializa com valores padrão.
void loadData() {
  EEPROM.get(0, dataStorage);
  if (dataStorage.teamName[0][0] == '\0' || dataStorage.teamName[0][0] == 0xFF) {
    Serial.println("EEPROM (Partida) nao inicializada. Configurando valores padrao...");
    strcpy(dataStorage.teamName[0], "Time A");
    strcpy(dataStorage.teamName[1], "Time B");
    resetData();
  }
}

// Salva a struct 'championshipData' na EEPROM, em um endereço após os dados da partida.
void saveChampionshipData() {
  EEPROM.put(sizeof(dataStorage), championshipData);
  EEPROM.commit();
}

// Carrega os dados do campeonato da EEPROM. Se for a primeira vez, zera as estatísticas.
void loadChampionshipData() {
  EEPROM.get(sizeof(dataStorage), championshipData);
  uint16_t check = 0xFFFF; // Valor típico de memória flash vazia.
  if (memcmp(&championshipData[0].points, &check, 2) == 0) {
      Serial.println("EEPROM (Campeonato) nao inicializada. Zerando estatísticas...");
      for(int i = 0; i < 2; i++) {
          championshipData[i].points = 0;
          championshipData[i].wins = 0;
          championshipData[i].draws = 0;
          championshipData[i].losses = 0;
      }
      saveChampionshipData();
  }
}

// Zera placares e pontos de jogadores da partida atual.
void resetData() {
  for (int t = 0; t < 2; t++) {
    dataStorage.teamScore[t] = 0;
    for (int p = 0; p < 11; p++) dataStorage.playerPoints[t][p] = 0;
  }
  saveData();
}

// ==================== FUNÇÕES MQTT ====================
// Publica o estado da partida atual (placar dos times e pontos dos jogadores) via MQTT.
void publishScores() {
  snprintf(topicBuffer, sizeof(topicBuffer), "/%s/%s/attrs", "TEF", "placar001");

  snprintf(msgBuffer, sizeof(msgBuffer), "TA|%d", dataStorage.teamScore[0]);
  MQTT.publish(topicBuffer, msgBuffer);
  snprintf(msgBuffer, sizeof(msgBuffer), "TB|%d", dataStorage.teamScore[1]);
  MQTT.publish(topicBuffer, msgBuffer);

  char player_buffer[150]; player_buffer[0] = '\0';
  char temp_buffer[80];

  snprintf(temp_buffer, sizeof(temp_buffer), "A_%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
    dataStorage.playerPoints[0][0], dataStorage.playerPoints[0][1], dataStorage.playerPoints[0][2],
    dataStorage.playerPoints[0][3], dataStorage.playerPoints[0][4], dataStorage.playerPoints[0][5],
    dataStorage.playerPoints[0][6], dataStorage.playerPoints[0][7], dataStorage.playerPoints[0][8],
    dataStorage.playerPoints[0][9], dataStorage.playerPoints[0][10]);
  strcat(player_buffer, temp_buffer);

  snprintf(temp_buffer, sizeof(temp_buffer), "_B_%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
    dataStorage.playerPoints[1][0], dataStorage.playerPoints[1][1], dataStorage.playerPoints[1][2],
    dataStorage.playerPoints[1][3], dataStorage.playerPoints[1][4], dataStorage.playerPoints[1][5],
    dataStorage.playerPoints[1][6], dataStorage.playerPoints[1][7], dataStorage.playerPoints[1][8],
    dataStorage.playerPoints[1][9], dataStorage.playerPoints[1][10]);
  strcat(player_buffer, temp_buffer);

  snprintf(msgBuffer, sizeof(msgBuffer), "j|%s", player_buffer);
  MQTT.publish(topicBuffer, msgBuffer);
}

// Publica o estado do campeonato (pontos e vitórias) via MQTT.
void publishChampionshipData() {
  snprintf(topicBuffer, sizeof(topicBuffer), "/%s/%s/attrs", "TEF", "placar001");
  snprintf(msgBuffer, sizeof(msgBuffer), "cpA|%d", championshipData[0].points); MQTT.publish(topicBuffer, msgBuffer);
  snprintf(msgBuffer, sizeof(msgBuffer), "cwA|%d", championshipData[0].wins);  MQTT.publish(topicBuffer, msgBuffer);
  snprintf(msgBuffer, sizeof(msgBuffer), "cpB|%d", championshipData[1].points); MQTT.publish(topicBuffer, msgBuffer);
  snprintf(msgBuffer, sizeof(msgBuffer), "cwB|%d", championshipData[1].wins);  MQTT.publish(topicBuffer, msgBuffer);
  Serial.println("Dados do campeonato publicados via MQTT.");
}

// Função executada sempre que uma mensagem é recebida em um tópico assinado.
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  if (length >= sizeof(msgBuffer)) return;
  memcpy(msgBuffer, payload, length);
  msgBuffer[length] = '\0';
  Serial.printf("Comando recebido: %s\n", msgBuffer);

  char* command_start = strchr(msgBuffer, '@');
  if (command_start == NULL) return;
  command_start++;

  if (strncmp(command_start, "resetChampionship", 17) == 0) {
    Serial.println("-> Ação: Resetar Campeonato");
    resetChampionship();
  }
  else if (strncmp(command_start, "reset", 5) == 0) {
    Serial.println("-> Ação: Resetar Partida");
    resetData();
    publishScores();
  } 
  else if (strncmp(command_start, "addPoint", 8) == 0 || strncmp(command_start, "removePoint", 11) == 0) {
    char* value_start = strchr(command_start, '|');
    if (!value_start) return;
    value_start++;
    int time = -1, jogador = -1;
    char* time_ptr = strstr(value_start, "time=");
    char* jogador_ptr = strstr(value_start, "jogador=");
    if (time_ptr && jogador_ptr) {
      sscanf(time_ptr + 5, "%d", &time);
      sscanf(jogador_ptr + 8, "%d", &jogador);
      if (time >= 0 && time <= 1 && jogador >= 0 && jogador <= 10) {
        selectedTeam = time;
        selectedPlayer = jogador;
        playerModeAdd = (strncmp(command_start, "addPoint", 8) == 0);
        Serial.printf("-> Ação: %s para Time %d, Jogador %d\n", playerModeAdd ? "Adicionar Ponto" : "Remover Ponto", time, jogador);
        applyPointChange();
      }
    }
  }
  else if (strncmp(command_start, "applyPenalty", 12) == 0) {
    char* value_start = strchr(command_start, '|');
    if (!value_start) return;
    value_start++;
    int time = -1, jogador = -1;
    char* time_ptr = strstr(value_start, "time=");
    char* jogador_ptr = strstr(value_start, "jogador=");
    if (time_ptr && jogador_ptr) {
        sscanf(time_ptr + 5, "%d", &time);
        sscanf(jogador_ptr + 8, "%d", &jogador);
        if (time >= 0 && time <= 1 && jogador >= 0 && jogador <= 10) {
            Serial.printf("-> Ação: Aplicar Penalidade no Time %d, Jogador %d\n", time, jogador);
            const int PENALTY_POINTS = 5;
            if (dataStorage.playerPoints[time][jogador] >= PENALTY_POINTS)
                 dataStorage.playerPoints[time][jogador] -= PENALTY_POINTS;
            else dataStorage.playerPoints[time][jogador] = 0;
            saveData();
            publishScores();
        }
    }
  }
  else if (strncmp(command_start, "finalizeMatch", 13) == 0) {
    Serial.println("-> Ação: Finalizar Partida");
    finalizeMatch();
  }
}

// Gerencia a conexão Wi-Fi e MQTT, tentando reconectar se necessário.
void checkConnections() { 
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi desconectado. Tentando reconectar...");
    WiFi.begin(SSID, PASSWORD);
  }
  if (!MQTT.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      Serial.print("Tentando conectar ao broker MQTT... ");
      if (MQTT.connect(ID_MQTT)) {
        Serial.println("Conectado!");
        MQTT.subscribe(TOPICO_CMD);
      } else {
        Serial.print("Falhou, estado: ");
        Serial.println(MQTT.state());
      }
    }
  }
}

// ==================== DESENHO DE TELAS NO OLED ====================
ScreenState lastScreen = SCREEN_MAIN;
int lastScore[2] = {-1, -1};
int lastPlayerPoints = -1;
bool lastMode = true;

// Desenha a tela principal com placares dos times.
void drawMainScreen() {
  if (lastScreen == SCREEN_MAIN && lastScore[0] == dataStorage.teamScore[0] && lastScore[1] == dataStorage.teamScore[1] && lastSelectedTeam == selectedTeam) return;
  display.clearDisplay();
  display.setTextWrap(false);
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(15, 0);  display.print(dataStorage.teamName[0]);
  display.setCursor(15,16);  display.print(dataStorage.teamScore[0]);
  display.setCursor(15,32);  display.print(dataStorage.teamName[1]);
  display.setCursor(15,48);  display.print(dataStorage.teamScore[1]);
  display.setTextSize(3);
  display.setCursor(0, selectedTeam == 0 ? 8 : 40);
  display.print(">");
  display.display();
  lastScreen = SCREEN_MAIN;
  lastScore[0] = dataStorage.teamScore[0];
  lastScore[1] = dataStorage.teamScore[1];
  lastSelectedTeam = selectedTeam;
}

// Desenha a tela de seleção de jogador.
void drawPlayerScreen() {
  int currentPoints = dataStorage.playerPoints[selectedTeam][selectedPlayer];
  if (lastScreen == SCREEN_PLAYER_SELECT && lastPlayerPoints == currentPoints && lastMode == playerModeAdd && lastSelectedPlayer == selectedPlayer) return;
  display.clearDisplay();
  display.setTextWrap(false);
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);  display.print(dataStorage.teamName[selectedTeam]);
  display.setCursor(0,12); display.print("Jogador "); display.print(selectedPlayer + 1);
  display.setCursor(0,24); display.print("Pontos: "); display.print(currentPoints);
  display.setCursor(0,40); display.print(playerModeAdd ? "[ADICIONAR]" : "[REMOVER]");
  display.display();
  lastScreen = SCREEN_PLAYER_SELECT;
  lastPlayerPoints = currentPoints;
  lastMode = playerModeAdd;
  lastSelectedPlayer = selectedPlayer;
}

// ==================== LÓGICA DO JOGO ====================
// Aplica a alteração de ponto (adicionar/remover) para o jogador e time selecionados.
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

// Compara placares, atualiza estatísticas do campeonato e reseta a partida.
void finalizeMatch() {
  if (dataStorage.teamScore[0] > dataStorage.teamScore[1]) {
    championshipData[0].points += 3; championshipData[0].wins++; championshipData[1].losses++;
  } else if (dataStorage.teamScore[1] > dataStorage.teamScore[0]) {
    championshipData[1].points += 3; championshipData[1].wins++; championshipData[0].losses++;
  } else {
    championshipData[0].points += 1; championshipData[1].points += 1;
    championshipData[0].draws++;      championshipData[1].draws++;
  }
  saveChampionshipData();
  publishChampionshipData();
  resetData();
  publishScores();
}

// Zera as estatísticas do campeonato.
void resetChampionship() {
  for (int i = 0; i < 2; i++) {
    championshipData[i].points = 0;
    championshipData[i].wins = 0;
    championshipData[i].draws = 0;
    championshipData[i].losses = 0;
  }
  saveChampionshipData();
  publishChampionshipData();
  Serial.println("Estatísticas do campeonato foram resetadas!");
}

// Retorna 'true' se o botão estiver pressionado (lógica de INPUT_PULLUP).
bool buttonPressed(Button btn) { return digitalRead(buttonPins[btn]) == LOW; }

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  loadData();
  loadChampionshipData();

  for (int i = 0; i < BTN_COUNT; i++) pinMode(buttonPins[i], INPUT_PULLUP);

  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(100);
  Serial.println("Wi-Fi conectado!");

  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, I2C_ADDRESS)) {
    Serial.println(F("Falha ao alocar SSD1306"));
    for (;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Wi-Fi Conectado!");
  display.display();

  MQTT.setServer(BROKER_MQTT, BROKER_PORT);
  MQTT.setCallback(mqtt_callback);

  drawMainScreen();
  publishScores();
  publishChampionshipData();
}

// ==================== LOOP ====================
void loop() {
  checkConnections();
  MQTT.loop();

  static unsigned long lastPress = 0;
  if (millis() - lastPress < 200) return; // Debounce de 200ms

  if (buttonPressed(BTN_UP)) { // Na tela principal: seleciona Time A. Na de jogador: jogador anterior.
    if (screen == SCREEN_MAIN) selectedTeam = 0;
    else selectedPlayer = (selectedPlayer + 10) % 11;
    lastPress = millis();
  } 
  else if (buttonPressed(BTN_DOWN)) { // Na tela principal: seleciona Time B. Na de jogador: próximo jogador.
    if (screen == SCREEN_MAIN) selectedTeam = 1;
    else selectedPlayer = (selectedPlayer + 1) % 11;
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

  if (screen == SCREEN_MAIN) drawMainScreen();
  else drawPlayerScreen();
}