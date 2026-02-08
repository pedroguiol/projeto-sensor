#include <WiFi.h>
#include <WiFiClientSecure.h> 
#include <PubSubClient.h>     
#include <ArduinoJson.h> 
#include <Adafruit_Fingerprint.h>
#include <Wire.h>
#include <HTTPClient.h>
#include <LITTLEFS.h>
#include <Preferences.h>
Preferences preferences;
#include "rgb_lcd.h"

#define SDA 21
#define SCL 22
#define PINO_RX_DO_ESP 16
#define PINO_TX_DO_ESP 17
rgb_lcd lcd;


HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

struct Aluno {
  char nome[17];      // Nome (16 letras + 1 para o fim da String)
  int matricula;      // A matrícula da universidade
  int idSensor;       // O ID (1, 2, 3...) que o AS608 reconhece
};
vector<Pessoa> lista; 
lista[0].push_back("Nobody",0,0);
// Configurações do Wi-Fi
const char* ssid = "NOBODY";
const char* password = "100Pedro";

// Detalhes do Broker HiveMQ
const char* mqtt_broker = "07d9a34421014e6598f8331685171c61.s1.eu.hivemq.cloud";
const int mqtt_port = 8883; 
const char* mqtt_usernameE = "ESP32";
const char* mqtt_passwordE = "ecJACUIPENSE1";

WiFiClientSecure espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);
  delay(1000);

  mySerial.begin(57600, SERIAL_8N1, PINO_RX_DO_ESP, PINO_TX_DO_ESP); 

  // Inicia o sensor usando a biblioteca Adafruit
  finger.begin(57600);

  if(finger.begin()) {
    Serial.println("Sensor encontrado");
  } else {
    Serial.println("Sensor não encontrado");
  }

  Wire.begin(SDA, SCL);
  lcd.begin(16, 2); // set up the LCD's number of columns and rows
  lcd.setRGB(255, 255, 0);
  lcd.setCursor(0, 0); // set the cursor to column 0, line 0
  // 1. Conectar Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    lcd.clear();
    lcd.print("Conectando na WI-FI");
  }
  lcd.clear(); // Apaga tudo na tela e volta o cursor para 0,0
  lcd.setCursor(0, 0); // Define a coluna 0, linha 0 (ou 0,1 para a segunda linha)

  Serial.println("\n[SUCESSO] Wi-Fi conectado!");
  lcd.print("[SUCESSO] Wi-Fi conectado");
  delay(2000);
  // 2. Configurar Segurança (SSL/TLS) - Obrigatório para HiveMQ Cloud
  espClient.setInsecure(); // Permite conectar sem validar o certificado raiz

  // 3. Configurar MQTT
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback); 
}

// Esta função é chamada quando o site envia algo

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("\nMensagem recebida no tópico: ");
  Serial.println(topic);
  
  String mensagem = "";
  for (int i = 0; i < length; i++) {
    mensagem += (char)payload[i];
  }

  if (String(topic) == "universidade/cadastro") {
    StaticJsonDocument<256> doc;
    deserializeJson(doc, mensagem);

    String nomeCompleto = doc["aluno"] | "Aluno"; 
    String primeiroNome;

    // Procura a posição do primeiro espaço
    int indiceEspaco = nomeCompleto.indexOf(' ');

    if (indiceEspaco != -1) {
        // Se achou um espaço, pega tudo que vem antes dele
        primeiroNome = nomeCompleto.subString(0, indiceEspaco);
    } else {
        // Se não tem espaço (nome único), usa o nome inteiro
        primeiroNome = nomeCompleto;
    }

    // Limite de segurança: se o primeiro nome sozinho ainda for maior que 16
    if (primeiroNome.length() > 16) {
        primeiroNome = primeiroNome.subString(0, 16);
    }

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sensor ativado, ");
    lcd.print(primeiroNome);
    
    lcd.setCursor(0, 1);
    lcd.print("Matricula:");
    lcd.print((int)doc["matricula"]);
    int matricula = (int)doc["matricula"];  
    delay(1000);
    int p;
    do {
      p = finger.getImage();
    
      delay(50); // Delay curto para não travar o loop
    } while (p != FINGERPRINT_OK);

    p = finger.image2Tz(1);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Coloque o dedo novamente");
    delay(1000);
    lcd.clear();

    do {
      p = finger.getImage();
    
      yield();
    } while (p != FINGERPRINT_OK);

    lcd.clear();
    lcd.print("Tire o dedo...");
    delay(1000);
    while (finger.getImage() != FINGERPRINT_NOFINGER) { 
      yield(); 
    }

    lcd.clear();
    lcd.print("Dedo novamente");
    do {
      p = finger.getImage();
      yield();
    } while (p != FINGERPRINT_OK);

    p = finger.image2Tz(2);
    if (p != FINGERPRINT_OK) return;

    p = finger.createModel();
    if (p == FINGERPRINT_OK) {
      Serial.println("Digitais combinam!");
      
      // Gerenciamento do ID persistente
      preferences.begin("sistema", false); 
      int idAtual = preferences.getInt("proximo_id", 1);
      
      if (finger.storeModel(idAtual) == FINGERPRINT_OK) {
        lcd.clear();
        lcd.print("Salvo no ID: ");
        lcd.print(idAtual);
        lista.push_back({nomeCompleto,matricula,idAtual});
        // Salva o próximo ID na Flash
        idAtual++;
        preferences.putInt("proximo_id", idAtual);
        
        // Aqui você envia pro banco de dados
        registrarPresenca(matricula, primeiroNome); 
      }
      preferences.end(); // FECHA O PREFERENCES
      
    } else {
      lcd.clear();
      lcd.print("Nao combinam!");
    }
  }
    
}


// Atualizamos a função para receber o ID E o Nome
void registrarPresenca(String nome, int matricula) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    // URL do seu Google Apps Script
    http.begin("https://script.google.com/macros/s/AKfycbz...SUA_URL.../exec"); 
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String dados;
    
    if(LittleFS.exists("/arquivo.txt")) {
      Serial.println("Arquivo existe");
      File file = LittleFS.open("/arquivo.txt", "r");

      if(!file || file.size() == 0) {
        Serial.println("Arquivo está vazio ou não pôde ser aberto");
      } else {
        Serial.print("Tamanho do arquivo: ");
        Serial.println(file.size());

        while(file.size() != 0) {
          string linha = file.readString();
          int32_t tamanho = linha.size();
          int8_t i = 0;
          String nomeArq;
          String matricula;
          while(linha[i] != ',') {
            nomeArq += linha[i];
          }

          while(linha[i] != '\n') {
            matricula += linha[i];
          }
          dados = "matricula=" + matricula + "&nome=" + nome;
          Serial.println("Enviando para o Google Sheets...");
          int httpResponseCode = http.POST(dados);
          
          if (httpResponseCode > 0) {
            Serial.println("Resposta do servidor: " + String(httpResponseCode));
          } else {
            Serial.println("Erro no envio POST");
          }
          http.end();
        }
      }
      file.close();

    } else {
      Serial.println("Arquivo não encontrado");
    }


    // Agora 'nome' é a variável que recebemos no parâmetro
    dados = "matricula=" + String(matricula) + "&nome=" + nome;
    
    Serial.println("Enviando para o Google Sheets...");
    int httpResponseCode = http.POST(dados);
    
    if (httpResponseCode > 0) {
      Serial.println("Resposta do servidor: " + String(httpResponseCode));
    } else {
      Serial.println("Erro no envio POST");
    }
    http.end();
  }
}

void dadosnaPlaca() {
    if (WiFi.status() == WL_CONNECTED && LittleFS.exists("/arquivo.txt")) {

      Serial.println("Arquivo existe");
      File file = LittleFS.open("/arquivo.txt", "r");
      HTTPClient http;
      // URL do seu Google Apps Script
      http.begin("https://script.google.com/macros/s/AKfycbz...SUA_URL.../exec"); 
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");
      String dados;

      if(!file || file.size() == 0) {
        Serial.println("Arquivo está vazio ou não pôde ser aberto");
      } else {
        Serial.print("Tamanho do arquivo: ");
        Serial.println(file.size());

        while(file.size() != 0) {
          string linha = file.readString();
          int32_t tamanho = linha.size();
          int8_t i = 0;
          String nomeArq;
          String matricula;
          while(linha[i] != ',') {
            nomeArq += linha[i];
          }

          while(linha[i] != '\n') {
            matricula += linha[i];
          }

          dados = "matricula=" + matricula + "&nome=" + nome;
          Serial.println("Enviando para o Google Sheets...");
          int httpResponseCode = http.POST(dados);
          
          if (httpResponseCode > 0) {
            Serial.println("Resposta do servidor: " + String(httpResponseCode));
          } else {
            Serial.println("Erro no envio POST");
          }
          http.end();
        }
      }
    file.close;
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Tentando conexão MQTT...");
    // CONSERTO: O ID do cliente deve ser único para não desconectar o site
    if (client.connect("ESP32_Placa_Fisica", mqtt_usernameE, mqtt_passwordE)) {
      Serial.println("Conectado!");
      // CONSERTO: Inscrito no tópico exato que o seu site publica
      client.subscribe("universidade/cadastro"); 
    } else {
      Serial.print("Falha, rc=");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void identificarAluno(int8_t p) {

  if(p > idAtual) {   // Colocar o idAtual como variável global
    Serial.println("Id inválido");
    return;
  } else {
    String aluno = lista[p].nome;
    int matricula = lista[p].matricula;

    lcd.cursor(0,0);
    lcd.print("Olá, ");
    lcd.print(aluno);
   
    if(WiFi.status() != WL_CONNECTED) {
    
      if(!LittleFS.begin(true)){ 
        Serial.println("Erro ao montar"); 
      }

      File file = LittleFS.open("/arquivo.txt", "w");
      file.println(lista[p].nome,lista[p].matricula);
      file.close();

    } else {
      registrarPresenca(aluno,matricula);
    }
    
  }

}

int8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return -1; // Sem dedo ou erro

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -1; // Erro de conversão

  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    Serial.print("Digital encontrada! ID #");
    Serial.print(finger.fingerID); // Retorna o ID
    Serial.print(" Confiança: ");
    Serial.println(finger.confidence); // Nível de confiança
    return finger.fingerID;
  }
  return -1; // Nenhuma correspondência
}


void loop() {
  if (!client.connected()) {
    reconnect();
  }

  int p = getFingerprintID();

  if(p != -1) {
    Serial.println("Dedo encontrado");
    identificarAluno(p);
  }

  client.loop(); // Mantém a conexão ativa processando o rádio Wi-Fi
}