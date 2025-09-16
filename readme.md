# Placar para jogos em ESP32 com MQTT e Display OLED

Este projeto consiste em um placar eletrônico totalmente funcional, construído com um microcontrolador ESP32. O sistema é capaz de gerenciar e exibir o placar de dois times, incluindo a pontuação individual de até 11 jogadores por equipe.

Além do controle físico através de botões, o placar é integrado à plataforma de IoT FIWARE, permitindo o monitoramento e controle remoto completo via API REST, utilizando ferramentas como o Postman. Os dados do placar são salvos na memória interna (EEPROM) para garantir persistência entre reinicializações.

A integração foi feita com o **FIWARE** e testada via **Postman**, conforme orientado pelo professor **Fábio Cabrini** em seu [repositório FIWARE Descomplicado](https://github.com/fabiocabrini/fiware).

------------------------------------------------------------------------

## 📌 Funcionalidades

-   Exibição em Tempo Real: Placar dos times e jogadores exibido em um display OLED.
-   Controle Físico: Interface com 6 botões para navegação, seleção e pontuação.
-   Persistência de Dados: O placar é salvo na EEPROM e recarregado automaticamente ao ligar.
-   Comunicação IoT: Publicação de todo o estado do placar em tempo real via protocolo MQTT.
-   API de Controle Remoto: Integração total com a plataforma FIWARE, permitindo que o placar
    - Seja controlado e monitorado através de requisições HTTP.
    - Adicionar/Remover pontos de jogadores específicos.
    - Resetar o placar.

------------------------------------------------------------------------

## 🛠️ Bibliotecas

-   **ESP32**
-   **WiFi.h** → Conexão à rede Wi-Fi\
-   **PubSubClient.h** → Comunicação MQTT\
-   **ArduinoJson.h** → Criação e leitura de mensagens JSON\
-   **Adafruit_GFX.h** → Gráficos básicos para o display\
-   **Adafruit_SSD1306.h** → Controle do display OLED

------------------------------------------------------------------------

## 🏗️ Arquitetura do Sistema

O sistema é dividido em três componentes principais que se comunicam de forma assíncrona:

Dispositivo (ESP32): Lê os botões, atualiza o display OLED e envia/recebe dados via MQTT.

Backend (FIWARE): Suíte de serviços em Docker, baseada no projeto FIWARE Descomplicado.

Eclipse Mosquitto: Broker MQTT.

IoT Agent for MQTT: Traduz mensagens MQTT para NGSI-v2 e vice-versa.

Orion Context Broker: O "cérebro" do sistema, armazena o contexto e expõe a API REST.

Cliente (Postman): Interação com a API do Orion, permitindo configurar, controlar e monitorar o placar remotamente.

Fluxo de dados:

ESP32 <--> Broker MQTT <--> IoT Agent <--> Orion Broker <--> Postman

------------------------------------------------------------------------

## 🛠️ Hardware Necessário

- **1x ESP32 DevKit V1 (DevKit-C V4)**  
  Microcontrolador principal responsável pela lógica, conexão Wi-Fi e comunicação MQTT.  

- **1x Display OLED SSD1306 (128x64, I2C, endereço 0x3C)**  
  Usado para exibir o placar dos times e jogadores em tempo real.  

- **1x Protoboard (meia protoboard)**  
  Para facilitar as conexões sem solda.  

- **6x Botões de pressão (push buttons)**  
  - **Botão 1 (esquerda)** – GPIO32  
  - **Botão 2 (baixo)** – GPIO26  
  - **Botão 3 (cima)** – GPIO25  
  - **Botão 4 (direita)** – GPIO33  
  - **Botão 5 (confirmar)** – GPIO27  
  - **Botão 6 (voltar)** – GPIO14  

- **Jumpers (fios de conexão)**  
  Para interligar o ESP32, OLED, botões e a protoboard.  

- **Fonte de alimentação (3.3V do ESP32)**  
  Alimenta o display OLED e os botões. 

------------------------------------------------------------------------
## ⚙️ Software e Dependências
Backend

Docker e Docker-Compose

Repositório FIWARE Descomplicado

Firmware (ESP32)

Arduino IDE ou PlatformIO (VS Code).

Bibliotecas Arduino:

Wire

Adafruit_GFX

Adafruit_SSD1306

EEPROM

WiFi

PubSubClient

Controle e Teste

Postman

## ▶️ Como Executar

1.  Instale as bibliotecas necessárias pela **Arduino IDE**:

    -   WiFi
    -   PubSubClient
    -   ArduinoJson
    -   Adafruit GFX
    -   Adafruit SSD1306

2.  Configure a rede Wi-Fi no código:

    ``` cpp
    const char* ssid = "NOME_DA_REDE";
    const char* password = "SENHA_DA_REDE";
    ```

3.  Compile e carregue o código no **ESP32**.

4.  Abra o **Serial Monitor** para acompanhar os logs.

## No Postman

1. Importe o [FIWARE Descomplicado](https://github.com/fabiocabrini/fiware)

2. Importe o [colocar ainda](link)

3. Faça o Health Check

4. Faça o Provisionamento do Service Group

5. Faça o Provisionamento do Dispositivo e o registre

------------------------------------------------------------------------

## 📌 Observações

-   O display utilizado é o **SSD1306** com comunicação **I2C** no
    endereço `0x3C`.\
-   Caso utilize outro modelo de OLED, ajuste a configuração no código.\
-   Para uso em produção, recomenda-se configurar um **broker MQTT
    privado**.


## Simulação do Código
Simule esse projeto em https://wokwi.com/projects/441933537299401729

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
  
