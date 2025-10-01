# Placar para jogos em ESP32 com MQTT e Display OLED

Este projeto implementa um **Placar Eletrônico Inteligente** utilizando o **ESP32**, integrado à plataforma **FIWARE** via **MQTT**, e capaz de gerenciar **partidas** e **campeonatos esportivos**.  
Mais que um simples placar, o sistema funciona como um **gêmeo digital** que pode ser controlado localmente ou remotamente, garantindo **operação híbrida** com persistência de dados.

---

## 📋 Descrição da Solução

O sistema foi projetado para unir **operações locais** (botões físicos + display OLED) com **operações remotas** (API REST/FIWARE + Postman).  
As principais funcionalidades incluem:

- **Gerenciamento de Partidas**: Controle do placar em tempo real para dois times.
- **Gamificação Individual **: Registro da pontuação de até **11 jogadores por equipe**.
- **Gerenciamento de Campeonato **: Atualização automática da tabela ao final da partida, com pontos, vitórias, empates e derrotas.
- **Sistema de Penalidades **: Dedução de pontos individuais aplicável remotamente.
- **API Remota Completa**: Via Postman é possível:
  - Adicionar/remover pontos de jogadores.
  - Resetar apenas a partida ou todo o campeonato.
  - Aplicar penalidades.
  - Finalizar partidas e atualizar a tabela do campeonato.
- **Persistência de Dados**: Todos os dados são salvos na **EEPROM** do ESP32.

---
## 🏗️ Arquitetura

A arquitetura segue a estrutura de componentes FIWARE:

1. **ESP32**: coleta entradas (botões) e publica os estados via MQTT.
2. **IoT Agent MQTT**: traduz mensagens para o Orion Context Broker.
3. **Orion Context Broker**: armazena e gerencia o contexto em tempo real.
4. **STH-Comet**: registra o histórico das partidas e estatísticas.
5. **Cliente (Postman)**: permite controlar o placar e o campeonato remotamente.

![Arquitetura de dados](https://github.com/404-Foundnt/IoT-Passa-a-Bola/blob/main/Modelo%20de%20arquitetura%20de%20dados/Arquitetura.png)

---

## 🔌 Diagrama de Ligações (Hardware)

### 📟 Display OLED (SSD1306 – I2C)

| OLED SSD1306 | ESP32 |
|--------------|-------|
| VCC          | 3.3V  |
| GND          | GND   |
| SDA          | 21    |
| SCL          | 22    |

### 🎮 Botões

| Botão      | Função       | ESP32 (GPIO) |
|------------|-------------|---------------|
| 1          | Esquerda    | 32            |
| 2          | Cima        | 25            |
| 3          | Direita     | 33            |
| 4          | Baixo       | 26            |
| 5          | Confirmar   | 27            |
| 6          | Retornar    | 14            |

### 🔋 Alimentação

- **3.3V** → VCC do OLED + botões  
- **GND** → GND do OLED + botões  

---

## 🛠️ Bibliotecas

-   **ESP32**
-   **WiFi.h** → Conexão à rede Wi-Fi\
-   **PubSubClient.h** → Comunicação MQTT\
-   **ArduinoJson.h** → Criação e leitura de mensagens JSON\
-   **Adafruit_GFX.h** → Gráficos básicos para o display\
-   **Adafruit_SSD1306.h** → Controle do display OLED

---

## 💻 Manual de Instalação e Operação

### 1. Hardware
- Monte o circuito conforme o diagrama de ligação.
- Conecte o ESP32 ao PC via USB.

### 2. Software – ESP32
1. Instale a [Arduino IDE](https://www.arduino.cc/en/software).
2. Adicione as bibliotecas:
   - `WiFi`
   - `PubSubClient`
   - `EEPROM`
   - `Wire`
   - `Adafruit_GFX`
   - `Adafruit_SSD1306`
3. Configure no código:
   - SSID e senha da rede Wi-Fi.
   - IP do Broker MQTT (VM FIWARE).
4. Compile e faça o upload para o ESP32.
5. Acompanhe logs pelo Serial Monitor.

### 3. Software – Nuvem (FIWARE)
1. Instale e configure o **FIWARE** em uma VM (Docker + Docker-Compose).
2. Configure os serviços principais:
   - Orion Context Broker (`1026`).
   - IoT Agent MQTT (`4041`).
   - Broker MQTT (porta `1883`).
   - STH-Comet (`8666`).
   
### 4. Postman – Controle Remoto
1. Importe a collection [IoT Placar de Jogos](https://github.com/404-Foundnt/IoT-Passa-a-Bola/blob/main/IoT%20Placar%20de%20Partidas.postman_collection.json).
2. Realize os seguintes passos:
   - Health Check.
   - Provisionamento do Service Group.
   - Provisionamento e registro do dispositivo.
3. Exemplos de comandos suportados:
   - `@reset` → Reseta apenas a partida atual.
   - `@resetChampionship` → Zera todas as estatísticas do campeonato.
   - `@addPoint|time=X&jogador=Y` → Adiciona ponto ao jogador Y do time X.
   - `@removePoint|time=X&jogador=Y` → Remove ponto do jogador Y do time X.
   - `@applyPenalty|time=X&jogador=Y` → Aplica penalidade (−5 pontos).
   - `@finalizeMatch` → Finaliza a partida e atualiza a tabela do campeonato.

---

## ⚙️ Software e Dependências

Backend

- Docker e Docker-Compose
- Repositório FIWARE Descomplicado
- Firmware (ESP32)
- Arduino IDE ou PlatformIO (VS Code).
- Bibliotecas Arduino:
- Wire
- Adafruit_GFX
- Adafruit_SSD1306
- EEPROM
- WiFi
- PubSubClient
- Postman

---

## ▶️ Operação

- Localmente: o placar pode ser controlado pelos **6 botões físicos** e visualizado no **display OLED**.  
- Remotamente: qualquer ação pode ser executada via **Postman** e **FIWARE**.  
- Os dados ficam salvos na EEPROM, garantindo persistência mesmo após desligar o dispositivo.

---

## Simulação do Código

Experimente no Wokwi: [Simulação Online](https://wokwi.com/projects/443531360703919105)

---

## Mais informações 
Para mais informações e explicações de como utilizar a ferramenta FIWARE e outras como MongoDB, Postman visite o repositório do professor Fabio Cabrini [FIWARE Descomplicado](https://github.com/fabiocabrini/fiware)

## Colaboradores

- Cesar Aaron Herrera
- Kaue Soares Madarazzo
- Rafael Seiji Aoke Arakaki
- Rafael Yuji Nakaya
- Nicolas Mendes dos Santos

## Agradecimentos

- Professor Fabio Cabrini (Disciplina: Edge Computing and Computer Systems, FIAP)
  
