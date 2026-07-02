# 👆 Sistema de Presença Biométrico Automatizado (ESP32)

Este projeto é um sistema inteligente e embarcado para automatizar listas de presença (chamadas) em salas de aula ou ambientes corporativos. Utilizando um microcontrolador **ESP32** e um **Sensor de Impressão Digital**, o sistema identifica o usuário e envia os dados de presença via Wi-Fi para uma planilha na nuvem (Google Sheets).

## 📋 Sobre o Projeto

O objetivo principal é eliminar as antigas listas de papel, otimizando o tempo e evitando fraudes. O sistema foi projetado para ser resiliente: caso o Wi-Fi caia, ele entra em **Modo Offline**, salvando os registros na memória interna do ESP32 (usando o LittleFS) e enviando os dados automaticamente para a nuvem assim que a conexão for reestabelecida.

## ✨ Funcionalidades Principais

* **Identificação Biométrica:** Leitura rápida e confiável usando sensor de digital.
* **Sincronização na Nuvem:** Envio de dados via requisição HTTP GET para um Web App do Google Apps Script (integrado ao Google Sheets).
* **Sistema de Fila Offline:** Se não houver internet, as presenças são gravadas na memória Flash (LittleFS) e processadas quando o Wi-Fi voltar.
* **Servidor Web Local (Captive/Local IP):** Interface HTML hospedada no próprio ESP32 para cadastro de novas digitais.
* **Feedback Visual:** Display LCD I2C informando o status da rede, instruções de cadastro e confirmação de presença com o nome do usuário.
* **Armazenamento de Vetor Local:** Banco de alunos armazenado localmente para resposta rápida.

## 🛠️ Hardware Utilizado

* **Placa:** ESP32 (com suporte a Wi-Fi e LittleFS)
* **Sensor Biométrico:** Módulo compatível com a biblioteca Adafruit Fingerprint (ex: AS608 / FPM10A)
* **Display:** Grove RGB LCD (I2C)
* **Conexões:**
  * Sensor Biométrico conectado à Serial 2 do ESP32 (Pinos RX: 16, TX: 17)
  * Display LCD conectado ao barramento I2C (SDA: 21, SCL: 22)

## 💻 Tecnologias e Bibliotecas

* **Linguagem:** C++ (Arduino IDE) / HTML / JavaScript
* **Bibliotecas ESP32:** `WiFi.h`, `WebServer.h`, `HTTPClient.h`, `WiFiClientSecure.h`, `Preferences.h`, `LittleFS.h`
* **Periféricos:** `Adafruit_Fingerprint` (Sensor), `rgb_lcd` (Display), `Wire.h` (I2C)
* **Cloud/Banco de Dados:** Google Apps Script (Planilhas). *Nota: O projeto também possui um frontend alternativo desenhado para integração com Supabase.*

## 🚀 Como Configurar e Rodar

### 1. Configuração de Rede e API
No arquivo principal (`.ino`), preencha as credenciais da sua rede Wi-Fi e a URL do seu Google Apps Script:
```cpp
const char* ssid = "SEU_WIFI";
const char* password = "SUA_SENHA";
String googleScriptURL = "SUA_URL_DO_GOOGLE_SCRIPT_AQUI";