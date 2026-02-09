#include <WiFi.h>
#include <WiFiClientSecure.h> 
#include <PubSubClient.h>     
#include <ArduinoJson.h> 
#include <Adafruit_Fingerprint.h>
#include <Wire.h>
#include <HTTPClient.h>
#include <FS.h>       // Inclua esta primeiro (Sistema de Arquivos Genérico)
#include <LittleFS.h> // Note que apenas o L, F e S são maiúsculos
#include <Preferences.h>
#include <vector> 
#include <string>
#include "rgb_lcd.h"

using namespace std;

Preferences preferences;

#define SDA 21
#define SCL 22
#define PINO_RX_DO_ESP 16
#define PINO_TX_DO_ESP 17

rgb_lcd lcd;

HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

struct Aluno {
  char nome[20];      
  int matricula;      
  int idSensor;       
};

// Vetor global
vector<Aluno> lista; 
int idAtual = 1; // Variável global para controle

// Configurações do Wi-Fi e MQTT
const char* ssid = "NOBODY";
const char* password = "100Pedro";
const char* mqtt_broker = "07d9a34421014e6598f8331685171c61.s1.eu.hivemq.cloud";
const int mqtt_port = 8883; 
const char* mqtt_usernameE = "ESP32";
const char* mqtt_passwordE = "ecJACUIPENSE1";

// URL do Google Script
const char* googleScriptURL = "https://script.google.com/macros/s/AKfycbz...SUA_URL.../exec";

WiFiClientSecure espClient;
PubSubClient client(espClient);

// --- FUNÇÕES DE PERSISTÊNCIA DO VETOR ---
// Salva a lista inteira no LittleFS para não perder os nomes se acabar a energia
void salvarBancoVector() {
  File f = LittleFS.open("/banco_alunos.dat", "w");
  if (f) {
    for (const auto& aluno : lista) {
      f.write((uint8_t*)&aluno, sizeof(Aluno));
    }
    f.close();
    Serial.println("Banco de alunos atualizado na Flash!");
  }
}

// Carrega a lista do LittleFS quando a placa liga
void carregarBancoVector() {
  File f = LittleFS.open("/banco_alunos.dat", "r");
  if (f) {
    lista.clear();
    Aluno temp;
    while (f.read((uint8_t*)&temp, sizeof(Aluno))) {
      lista.push_back(temp);
    }
    f.close();
    Serial.println("Banco de alunos carregado! Total: " + String(lista.size()));
  }
}
// ----------------------------------------

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Inicia LittleFS
  if(!LittleFS.begin(true)){
    Serial.println("Erro ao montar LittleFS");
    return;
  }

  carregarBancoVector();

  // Recupera o último ID usado
  preferences.begin("sistema", false);
  idAtual = preferences.getInt("proximo_id", 1);
  preferences.end();

  mySerial.begin(57600, SERIAL_8N1, PINO_RX_DO_ESP, PINO_TX_DO_ESP); 
  finger.begin(57600);

  if(finger.verifyPassword()) {
    Serial.println("Sensor encontrado");
  } else {
    Serial.println("Sensor não encontrado");
  }

  Wire.begin(SDA, SCL);
  lcd.begin(16, 2);
  lcd.setRGB(255, 255, 0);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    lcd.clear();
    lcd.print("Conectando Wi-Fi");
    Serial.println("Conectando Wi-Fi");
  }
  
  lcd.clear();
  lcd.print("Wi-Fi OK!");
  Serial.println("Wi-Fi OK!");
  delay(1000);

  espClient.setInsecure();
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback); 
}



// Função auxiliar para enviar dados
bool enviarParaGoogle(String nome, int matricula) {
  HTTPClient http;
  http.begin(googleScriptURL); 
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  String dados = "matricula=" + String(matricula) + "&nome=" + nome;
  int httpResponseCode = http.POST(dados);
  http.end();

  if (httpResponseCode > 0) {
    Serial.println("Enviado: " + String(httpResponseCode));
    return true;
  }
  Serial.println("Erro envio: " + String(httpResponseCode));
  return false;
}

// Função para processar a fila offline
void processarFilaOffline() {
  // Só tenta se tiver Wi-Fi e se o arquivo existir
  if (WiFi.status() == WL_CONNECTED && LittleFS.exists("/offline.txt")) {
    Serial.println("Sincronizando dados offline...");
    File file = LittleFS.open("/offline.txt", "r");
    
    // Vamos criar um arquivo temporário para salvar o que falhar (se houver)
    // Mas para simplificar: lemos tudo, enviamos, se der erro paramos e apagamos o enviado.
    // Estratégia simples: Ler linha a linha.
    
    // Nota: Ler e escrever no mesmo arquivo é complexo. 
    // O ideal é ler para a RAM, enviar e depois limpar o arquivo.
    
    while(file.available()) {
      String linha = file.readStringUntil('\n');
      if (linha.length() > 5) { // Filtra linhas vazias
        int commaIndex = linha.indexOf(',');
        if (commaIndex != -1) {
          String matriculaStr = linha.substring(0, commaIndex);
          String nomeStr = linha.substring(commaIndex + 1);
          
          Serial.print("Sincronizando: "); Serial.println(nomeStr);
          enviarParaGoogle(nomeStr, matriculaStr.toInt());
          delay(500); // Pausa para não bloquear o Google
        }
      }
    }
    file.close();
    
    // Assume que enviou tudo e limpa o arquivo
    LittleFS.remove("/offline.txt");
    Serial.println("Sincronização concluída.");
  }
}

// Função principal de registro
void registrarPresenca(String nome, int matricula) {
  bool enviado = false;

  // 1. Tenta enviar Online
  if (WiFi.status() == WL_CONNECTED) {
    enviado = enviarParaGoogle(nome, matricula);
  }

  // 2. Se falhar (sem wifi ou erro no server), salva Offline
  if (!enviado) {
    Serial.println("Salvando offline...");
    // AQUI É "a" (APPEND) E NÃO "w" (WRITE)
    File f = LittleFS.open("/offline.txt", "a"); 
    if (f) {
      // Salva no formato: matricula,nome
      f.printf("%d,%s\n", matricula, nome.c_str());
      f.close();
      lcd.setCursor(0, 1);
      lcd.print("Salvo Offline   ");
      Serial.println("Salvo offline ");
    } else {
      Serial.println("Erro ao abrir arquivo offline");
    }
  } else {
    lcd.setCursor(0, 1);
    lcd.print("Registro Enviado");
    Serial.println("Registro Enviado");
  }
}

void callback(char* topic, uint8_t* payload, unsigned int length) {
  String mensagem = "";
  for (int i = 0; i < length; i++) {
    mensagem += (char)payload[i];
  }

  if (String(topic) == "universidade/cadastro") {
    StaticJsonDocument<256> doc;
    deserializeJson(doc, mensagem);

    String nomeCompleto = doc["aluno"] | "Aluno"; 
    int matricula = doc["matricula"];
    
    // Tratamento do nome
    String primeiroNome;
    int indiceEspaco = nomeCompleto.indexOf(' ');
    if (indiceEspaco != -1) {
       primeiroNome = nomeCompleto.substring(0, indiceEspaco); // CORREÇÃO: substring (minúsculo)
    } else {
       primeiroNome = nomeCompleto;
    }
    if (primeiroNome.length() > 16) primeiroNome = primeiroNome.substring(0, 16);


    lcd.clear();
    lcd.print("Cadastrar: ");
    lcd.setCursor(0, 1);
    lcd.print(primeiroNome);
    Serial.print("Cadastrar: ");
    Serial.println(primeiroNome);
    delay(1000);
    
    int p = -1;
    
    // Captura 1
    lcd.clear(); lcd.print("Coloque o dedo");
    while ((p = finger.getImage()) != FINGERPRINT_OK) { yield(); }
    finger.image2Tz(1);

    // Espera tirar
    lcd.clear(); lcd.print("Tire o dedo");
    delay(1000);
    while (finger.getImage() != FINGERPRINT_NOFINGER) { yield(); }

    // Captura 2
    lcd.clear(); lcd.print("Dedo novamente");
    while ((p = finger.getImage()) != FINGERPRINT_OK) { yield(); }
    finger.image2Tz(2);

    if (finger.createModel() == FINGERPRINT_OK) {
      // Usa o ID GLOBAL para salvar
      if (finger.storeModel(idAtual) == FINGERPRINT_OK) {
        lcd.clear();
        lcd.print("Salvo ID: ");
        lcd.print(idAtual);
        Serial.print("Salvo ID: ");
        Serial.println(idAtual);
        
        // 1. Adiciona ao vetor na RAM
        Aluno novoAluno;
        strncpy(novoAluno.nome, primeiroNome.c_str(), 19);
        novoAluno.matricula = matricula;
        novoAluno.idSensor = idAtual;
        lista.push_back(novoAluno);
        
        // 2. Atualiza o arquivo de backup do vetor
        salvarBancoVector();

        // 3. Atualiza o ID para o próximo
        idAtual++;
        preferences.begin("sistema", false);
        preferences.putInt("proximo_id", idAtual);
        preferences.end();
        
        // 4. Registra a presença do cadastro
        registrarPresenca(primeiroNome, matricula); 
      }
    } else {
      lcd.clear(); lcd.print("Erro Digital");
      Serial.println("Erro Digital");
    }
  }
}

void identificarAluno(int idEncontrado) {

  bool encontrado = false;
  Serial.println(encontrado);
  for (const auto& aluno : lista) {
    if (aluno.idSensor == idEncontrado) {
      lcd.clear();
      lcd.print("Oi, ");
      lcd.print(aluno.nome);
      Serial.print("oi, ");
      Serial.println(aluno.nome);
      
      registrarPresenca(String(aluno.nome), aluno.matricula);
      encontrado = true;
      break;
    } 
  }
  
  if (!encontrado) {
    Serial.println("Chegou aqui");
    lcd.clear();
    lcd.print("ID "); lcd.print(idEncontrado);
    lcd.setCursor(0,1);
    lcd.println("Nao cadastrado");
    Serial.print("Id: ");
    Serial.println("Nao cadastrado");
  }
  delay(2000);
  lcd.clear();
}

int getFingerprintID() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    return finger.fingerID;
  }

  if (p == FINGERPRINT_NOTFOUND) return -2;
  return -1;
}

void reconnect() {
  if (!client.connected()) {
    // Tenta reconectar (non-blocking se possível, mas aqui usamos while simples)
    if (client.connect("ESP32_Biometria", mqtt_usernameE, mqtt_passwordE)) {
      Serial.println("MQTT Conectado");
      client.subscribe("universidade/cadastro"); 
    }
  }
}

unsigned long ultimoSync = 0;

void loop() {
  // 1. Mantém MQTT
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  // 2. Verifica Sensor Biométrico
  int id = getFingerprintID();
  Serial.println(id);
  if (id != -1 && id != -2) {
    Serial.println("Id diferente de -1");
    identificarAluno(id);
  } 

  if(id == -2){
    lcd.clear();
    lcd.setCursor(0,1);
    lcd.println("Nao cadastrado");
    Serial.print("ID: ");
    Serial.println("Nao cadastrado!");
    delay(2000);
    lcd.clear();

    while (finger.getImage() != FINGERPRINT_NOFINGER) {
      client.loop(); // Mantém o MQTT vivo enquanto espera tirar o dedo
      yield();
    }
  
  }

  // 3. Processa dados Offline a cada 30 segundos (para não travar o sensor)
  if (millis() - ultimoSync > 30000) {
    processarFilaOffline();
    ultimoSync = millis();
  } 
}
